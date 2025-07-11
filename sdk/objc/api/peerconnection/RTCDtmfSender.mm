/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCDtmfSender+Private.h"

#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

#include "api/scoped_refptr.h"
#include "rtc_base/checks.h"
#include "rtc_base/time_utils.h"

@implementation RTC_OBJC_TYPE (RTCDtmfSender) {
  webrtc::scoped_refptr<webrtc::DtmfSenderInterface> _nativeDtmfSender;
}

- (BOOL)canInsertDtmf {
  return _nativeDtmfSender->CanInsertDtmf();
}

- (BOOL)insertDtmf:(nonnull NSString *)tones
          duration:(NSTimeInterval)duration
      interToneGap:(NSTimeInterval)interToneGap {
  RTC_DCHECK(tones != nil);

  int durationMs = static_cast<int>(duration * webrtc::kNumMillisecsPerSec);
  int interToneGapMs =
      static_cast<int>(interToneGap * webrtc::kNumMillisecsPerSec);
  return _nativeDtmfSender->InsertDtmf(
      [NSString stdStringForString:tones], durationMs, interToneGapMs);
}

- (nonnull NSString *)remainingTones {
  return [NSString stringForStdString:_nativeDtmfSender->tones()];
}

- (NSTimeInterval)duration {
  return static_cast<NSTimeInterval>(_nativeDtmfSender->duration()) /
      webrtc::kNumMillisecsPerSec;
}

- (NSTimeInterval)interToneGap {
  return static_cast<NSTimeInterval>(_nativeDtmfSender->inter_tone_gap()) /
      webrtc::kNumMillisecsPerSec;
}

- (NSString *)description {
  return
      [NSString stringWithFormat:
                    @"RTC_OBJC_TYPE(RTCDtmfSender) {\n  remainingTones: %@\n  "
                    @"duration: %f sec\n  interToneGap: %f sec\n}",
                    [self remainingTones],
                    [self duration],
                    [self interToneGap]];
}

#pragma mark - Private

- (webrtc::scoped_refptr<webrtc::DtmfSenderInterface>)nativeDtmfSender {
  return _nativeDtmfSender;
}

- (instancetype)initWithNativeDtmfSender:
    (webrtc::scoped_refptr<webrtc::DtmfSenderInterface>)nativeDtmfSender {
  NSParameterAssert(nativeDtmfSender);
  self = [super init];
  if (self) {
    _nativeDtmfSender = nativeDtmfSender;
    RTCLogInfo(@"RTC_OBJC_TYPE(RTCDtmfSender)(%p): created DTMF sender: %@",
               self,
               self.description);
  }
  return self;
}
@end
