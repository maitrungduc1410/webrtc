/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/sframe_video_receive_delegate.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/sframe/sframe_decryptor_interface.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/sframe/mock_sframe_media_decryptor.h"
#include "modules/sframe/sframe_decrypt_error_reporter.h"
#include "modules/sframe/sframe_decryption_config.h"
#include "modules/sframe/sframe_media_decryptor_interface.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::SizeIs;

// Ciphertext and plaintext are arbitrary: the decryptor is a mock, so the test
// only checks what the delegate passes to it and does with its result.
constexpr std::array<uint8_t, 4> kCiphertext = {0xaa, 0xbb, 0xcc, 0xdd};
constexpr std::array<uint8_t, 3> kPlaintext = {1, 2, 3};

// Action for MockSframeMediaDecryptor::Decrypt() that succeeds with
// `kPlaintext`.
SframeDecryptResult DecryptToPlaintext(
    std::span<const uint8_t> /*encrypted_frame*/,
    std::span<const uint8_t> /*additional_data*/,
    std::span<uint8_t> frame) {
  std::ranges::copy(kPlaintext, frame.begin());
  return SframeDecryptSuccess{.bytes_written = kPlaintext.size()};
}

std::unique_ptr<RtpPacketReceived> CreatePacket(
    std::span<const uint8_t> payload) {
  auto packet = std::make_unique<RtpPacketReceived>();
  packet->SetPayload(payload);
  return packet;
}

scoped_refptr<NiceMock<MockSframeMediaDecryptor>> CreateDecryptor() {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      make_ref_counted<NiceMock<MockSframeMediaDecryptor>>();
  ON_CALL(*decryptor, GetMaxPlaintextByteSize).WillByDefault(ReturnArg<0>());
  return decryptor;
}

// Returns a callback that appends the reported errors to `errors`.
SframeDecryptErrorCallback CollectErrors(
    std::vector<SframeDecryptError>& errors) {
  return [&errors](SframeDecryptError error) {
    errors.push_back(std::move(error));
  };
}

// Returns a config with `decryptor` and a reporter that appends the reported
// errors to `errors`.
SframeDecryptionConfig CreateConfig(
    scoped_refptr<MockSframeMediaDecryptor> decryptor,
    std::vector<SframeDecryptError>& errors) {
  return {.decryptor = std::move(decryptor),
          .error_reporter =
              SframeDecryptErrorReporter::Create(CollectErrors(errors))};
}

TEST(SframeVideoReceiveDelegateTest, DecryptPacketReplacesPayload) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors));
  EXPECT_CALL(*decryptor, Decrypt(ElementsAreArray(kCiphertext), _, _))
      .WillOnce(DecryptToPlaintext);

  std::unique_ptr<RtpPacketReceived> packet =
      delegate.DecryptPacket(CreatePacket(kCiphertext));

  ASSERT_THAT(packet, NotNull());
  EXPECT_THAT(packet->payload(), ElementsAreArray(kPlaintext));
  EXPECT_THAT(errors, IsEmpty());
}

TEST(SframeVideoReceiveDelegateTest, DecryptPacketDropsPadding) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors));
  EXPECT_CALL(*decryptor, Decrypt).WillOnce(DecryptToPlaintext);
  std::unique_ptr<RtpPacketReceived> encrypted = CreatePacket(kCiphertext);
  ASSERT_TRUE(encrypted->SetPadding(4));

  std::unique_ptr<RtpPacketReceived> packet =
      delegate.DecryptPacket(std::move(encrypted));

  ASSERT_THAT(packet, NotNull());
  EXPECT_THAT(packet->payload(), ElementsAreArray(kPlaintext));
  EXPECT_EQ(packet->padding_size(), 0u);
}

TEST(SframeVideoReceiveDelegateTest,
     DecryptPacketAfterLargerPacketKeepsOnlyItsPlaintext) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors));
  const std::vector<uint8_t> kLargeCiphertext(16, 0xaa);
  const std::vector<uint8_t> kLargePlaintext(8, 0x55);
  ASSERT_GT(kLargePlaintext.size(), kPlaintext.size());
  EXPECT_CALL(*decryptor, Decrypt)
      .WillOnce([&](std::span<const uint8_t> /*encrypted_frame*/,
                    std::span<const uint8_t> /*additional_data*/,
                    std::span<uint8_t> frame) -> SframeDecryptResult {
        std::ranges::copy(kLargePlaintext, frame.begin());
        return SframeDecryptSuccess{.bytes_written = kLargePlaintext.size()};
      })
      .WillOnce(DecryptToPlaintext);
  ASSERT_THAT(delegate.DecryptPacket(CreatePacket(kLargeCiphertext)),
              NotNull());

  std::unique_ptr<RtpPacketReceived> packet =
      delegate.DecryptPacket(CreatePacket(kCiphertext));

  ASSERT_THAT(packet, NotNull());
  EXPECT_THAT(packet->payload(), ElementsAreArray(kPlaintext));
}

TEST(SframeVideoReceiveDelegateTest, FailedPacketIsDroppedAndReported) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors));
  EXPECT_CALL(*decryptor, Decrypt)
      .WillOnce(Return(SframeDecryptFailure::Authentication()));

  EXPECT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext)), IsNull());

  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_EQ(errors[0].error_type, SframeDecryptErrorType::kAuthentication);
}

TEST(SframeVideoReceiveDelegateTest, ReportedFailureCarriesTheKeyId) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors));
  EXPECT_CALL(*decryptor, Decrypt)
      .WillOnce(Return(SframeDecryptFailure::KeyId(7)));

  EXPECT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext)), IsNull());

  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_EQ(errors[0].error_type, SframeDecryptErrorType::kKeyId);
  EXPECT_EQ(errors[0].key_id, 7u);
}

TEST(SframeVideoReceiveDelegateTest, RepeatedFailureIsReportedOnce) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors));
  EXPECT_CALL(*decryptor, Decrypt)
      .Times(2)
      .WillRepeatedly(Return(SframeDecryptFailure::Authentication()));

  EXPECT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext)), IsNull());
  EXPECT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext)), IsNull());

  EXPECT_THAT(errors, SizeIs(1));
}

TEST(SframeVideoReceiveDelegateTest, FailureAfterSuccessIsReportedAgain) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors));
  EXPECT_CALL(*decryptor, Decrypt)
      .WillOnce(Return(SframeDecryptFailure::Authentication()))
      .WillOnce(DecryptToPlaintext)
      .WillOnce(Return(SframeDecryptFailure::Authentication()));

  ASSERT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext)), IsNull());
  ASSERT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext)), NotNull());
  EXPECT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext)), IsNull());

  EXPECT_THAT(errors, SizeIs(2));
}

}  // namespace
}  // namespace webrtc
