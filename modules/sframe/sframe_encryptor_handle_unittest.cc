/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/sframe/sframe_encryptor_handle.h"

#include <cstdint>
#include <vector>

#include "api/make_ref_counted.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "modules/sframe/mock_sframe_media_encryptor.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::Return;

constexpr uint64_t kKeyId = 7;

TEST(SframeEncryptorHandleTest, SetsKeyOnEncryptor) {
  scoped_refptr<MockSframeMediaEncryptor> encryptor =
      make_ref_counted<MockSframeMediaEncryptor>();
  scoped_refptr<SframeEncryptorHandle> handle =
      SframeEncryptorHandle::Create(encryptor);
  const std::vector<uint8_t> key_material(16, 0xab);
  EXPECT_CALL(*encryptor,
              SetEncryptionKey(kKeyId, ElementsAreArray(key_material)))
      .WillOnce(Return(RTCError::OK()));

  EXPECT_TRUE(handle->SetEncryptionKey(kKeyId, key_material).ok());
}

TEST(SframeEncryptorHandleTest, ReportsKeyUpdateError) {
  scoped_refptr<MockSframeMediaEncryptor> encryptor =
      make_ref_counted<MockSframeMediaEncryptor>();
  scoped_refptr<SframeEncryptorHandle> handle =
      SframeEncryptorHandle::Create(encryptor);
  EXPECT_CALL(*encryptor, SetEncryptionKey)
      .WillOnce(Return(RTCError::InternalError("Bad key")));

  EXPECT_EQ(
      handle->SetEncryptionKey(kKeyId, std::vector<uint8_t>(16, 0xab)).type(),
      RTCErrorType::INTERNAL_ERROR);
}

}  // namespace
}  // namespace webrtc
