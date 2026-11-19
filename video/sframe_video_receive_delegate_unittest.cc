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

#include "api/frame_transformer_interface.h"
#include "api/make_ref_counted.h"
#include "api/rtp_header_extension_id.h"
#include "api/rtp_headers.h"
#include "api/scoped_refptr.h"
#include "api/sframe/sframe_decryptor_interface.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame_metadata.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_rotation.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
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

constexpr uint32_t kSsrc = 1234;
constexpr uint8_t kPayloadType = 98;
// Whole seconds, so that they convert exactly to the UQ32.32 and Q32.32 fixed
// point of the extension.
constexpr Timestamp kCaptureTime = Timestamp::Seconds(5);
constexpr TimeDelta kCaptureClockOffset = TimeDelta::Seconds(2);
constexpr AbsoluteCaptureTime kAbsoluteCaptureTime = {
    .absolute_capture_timestamp = uint64_t{kCaptureTime.seconds()} << 32,
    .estimated_capture_clock_offset = kCaptureClockOffset.seconds() << 32};

// Ciphertext and plaintext are arbitrary: the decryptor is a mock, so the test
// only checks what the delegate passes to it and does with its result.
constexpr std::array<uint8_t, 4> kCiphertext = {0xaa, 0xbb, 0xcc, 0xdd};
constexpr std::array<uint8_t, 3> kPlaintext = {1, 2, 3};

constexpr SframeVideoReceiveDelegate::FrameInfo kVp8Frame = {
    .codec = kVideoCodecVP8};

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
  RtpHeaderExtensionMap extensions;
  extensions.Register<AbsoluteCaptureTimeExtension>(RtpHeaderExtensionId(1));
  extensions.Register<VideoOrientation>(RtpHeaderExtensionId(2));
  extensions.Register<VideoContentTypeExtension>(RtpHeaderExtensionId(3));

  auto packet = std::make_unique<RtpPacketReceived>(&extensions);
  packet->SetSsrc(kSsrc);
  packet->SetPayloadType(kPayloadType);
  packet->SetSequenceNumber(11);
  packet->SetTimestamp(9000);
  packet->set_arrival_time(Timestamp::Millis(17));
  packet->SetExtension<AbsoluteCaptureTimeExtension>(kAbsoluteCaptureTime);
  packet->SetExtension<VideoOrientation>(kVideoRotation_90);
  packet->SetExtension<VideoContentTypeExtension>(
      VideoContentType::SCREENSHARE);
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
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors), kSsrc);
  EXPECT_CALL(*decryptor, Decrypt(ElementsAreArray(kCiphertext), _, _))
      .WillOnce(DecryptToPlaintext);

  std::unique_ptr<RtpPacketReceived> packet =
      delegate.DecryptPacket(CreatePacket(kCiphertext), kVp8Frame);

  ASSERT_THAT(packet, NotNull());
  EXPECT_THAT(packet->payload(), ElementsAreArray(kPlaintext));
  EXPECT_THAT(errors, IsEmpty());
}

TEST(SframeVideoReceiveDelegateTest, DecryptPacketDropsPadding) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors), kSsrc);
  EXPECT_CALL(*decryptor, Decrypt).WillOnce(DecryptToPlaintext);
  std::unique_ptr<RtpPacketReceived> encrypted = CreatePacket(kCiphertext);
  ASSERT_TRUE(encrypted->SetPadding(4));

  std::unique_ptr<RtpPacketReceived> packet =
      delegate.DecryptPacket(std::move(encrypted), kVp8Frame);

  ASSERT_THAT(packet, NotNull());
  EXPECT_THAT(packet->payload(), ElementsAreArray(kPlaintext));
  EXPECT_EQ(packet->padding_size(), 0u);
}

TEST(SframeVideoReceiveDelegateTest,
     DecryptPacketAfterLargerPacketKeepsOnlyItsPlaintext) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors), kSsrc);
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
  ASSERT_THAT(delegate.DecryptPacket(CreatePacket(kLargeCiphertext), kVp8Frame),
              NotNull());

  std::unique_ptr<RtpPacketReceived> packet =
      delegate.DecryptPacket(CreatePacket(kCiphertext), kVp8Frame);

  ASSERT_THAT(packet, NotNull());
  EXPECT_THAT(packet->payload(), ElementsAreArray(kPlaintext));
}

TEST(SframeVideoReceiveDelegateTest, FailedPacketIsDroppedAndReported) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors), kSsrc);
  EXPECT_CALL(*decryptor, Decrypt)
      .WillOnce(Return(SframeDecryptFailure::Authentication()));

  EXPECT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext), kVp8Frame),
              IsNull());

  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_EQ(errors[0].error_type, SframeDecryptErrorType::kAuthentication);
  EXPECT_THAT(errors[0].frame, NotNull());
}

TEST(SframeVideoReceiveDelegateTest, ReportedFailureCarriesTheKeyId) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors), kSsrc);
  EXPECT_CALL(*decryptor, Decrypt)
      .WillOnce(Return(SframeDecryptFailure::KeyId(7)));

  EXPECT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext), kVp8Frame),
              IsNull());

  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_EQ(errors[0].error_type, SframeDecryptErrorType::kKeyId);
  EXPECT_EQ(errors[0].key_id, 7u);
}

TEST(SframeVideoReceiveDelegateTest, ReportedPacketCarriesTheCiphertext) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors), kSsrc);
  EXPECT_CALL(*decryptor, Decrypt)
      .WillOnce(Return(SframeDecryptFailure::KeyId(7)));

  ASSERT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext), kVp8Frame),
              IsNull());

  ASSERT_THAT(errors, SizeIs(1));
  EXPECT_EQ(errors[0].key_id, 7u);
  ASSERT_THAT(errors[0].frame, NotNull());
  const auto& frame =
      static_cast<const TransformableVideoFrameInterface&>(*errors[0].frame);
  EXPECT_THAT(frame.GetData(), ElementsAreArray(kCiphertext));
  EXPECT_EQ(frame.GetSsrc(), kSsrc);
  EXPECT_EQ(frame.GetPayloadType(), kPayloadType);
  EXPECT_EQ(frame.GetMimeType(), "video/VP8");
  EXPECT_EQ(frame.ReceiveTime(), Timestamp::Millis(17));
  EXPECT_EQ(frame.CaptureTime(), kCaptureTime);
  EXPECT_EQ(frame.SenderCaptureTimeOffset(), kCaptureClockOffset);
  EXPECT_FALSE(frame.IsKeyFrame());

  const VideoFrameMetadata metadata = frame.Metadata();
  EXPECT_EQ(metadata.GetSsrc(), kSsrc);
  EXPECT_EQ(metadata.GetCodec(), kVideoCodecVP8);
  EXPECT_EQ(metadata.GetRotation(), kVideoRotation_90);
  EXPECT_EQ(metadata.GetContentType(), VideoContentType::SCREENSHARE);
  EXPECT_EQ(metadata.GetFrameType(), VideoFrameType::kEmptyFrame);
}

TEST(SframeVideoReceiveDelegateTest, ReportedPacketOfKeyFrameIsKeyFrame) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors), kSsrc);
  EXPECT_CALL(*decryptor, Decrypt)
      .WillOnce(Return(SframeDecryptFailure::Authentication()));

  ASSERT_THAT(
      delegate.DecryptPacket(CreatePacket(kCiphertext),
                             {.codec = kVideoCodecVP8, .is_key_frame = true}),
      IsNull());

  ASSERT_THAT(errors, SizeIs(1));
  ASSERT_THAT(errors[0].frame, NotNull());
  const auto& frame =
      static_cast<const TransformableVideoFrameInterface&>(*errors[0].frame);
  EXPECT_TRUE(frame.IsKeyFrame());
  EXPECT_EQ(frame.Metadata().GetFrameType(), VideoFrameType::kVideoFrameKey);
}

TEST(SframeVideoReceiveDelegateTest, RepeatedFailureIsReportedOnce) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors), kSsrc);
  EXPECT_CALL(*decryptor, Decrypt)
      .Times(2)
      .WillRepeatedly(Return(SframeDecryptFailure::Authentication()));

  EXPECT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext), kVp8Frame),
              IsNull());
  EXPECT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext), kVp8Frame),
              IsNull());

  EXPECT_THAT(errors, SizeIs(1));
}

TEST(SframeVideoReceiveDelegateTest, FailureAfterSuccessIsReportedAgain) {
  scoped_refptr<NiceMock<MockSframeMediaDecryptor>> decryptor =
      CreateDecryptor();
  std::vector<SframeDecryptError> errors;
  SframeVideoReceiveDelegate delegate(CreateConfig(decryptor, errors), kSsrc);
  EXPECT_CALL(*decryptor, Decrypt)
      .WillOnce(Return(SframeDecryptFailure::Authentication()))
      .WillOnce(DecryptToPlaintext)
      .WillOnce(Return(SframeDecryptFailure::Authentication()));

  ASSERT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext), kVp8Frame),
              IsNull());
  ASSERT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext), kVp8Frame),
              NotNull());
  EXPECT_THAT(delegate.DecryptPacket(CreatePacket(kCiphertext), kVp8Frame),
              IsNull());

  EXPECT_THAT(errors, SizeIs(2));
}

}  // namespace
}  // namespace webrtc
