/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCLogging.h"
#import "sdk/objc/base/RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

// Receives a log line in the default format and its severity.
typedef void (^RTCLogSinkHandler)(NSString *message,
                                  RTCLoggingSeverity severity);

// Configuration for RTCInitializeLogging(). Corresponds to
// webrtc::LoggingConfig, which also provides the default values.
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCLoggingConfig) : NSObject

// The minimum severity of the messages that are written to the debug output,
// such as stderr. By default, debug builds of WebRTC write messages from
// RTCLoggingSeverityInfo and release builds write none.
@property(nonatomic, assign) RTCLoggingSeverity debugSeverity;

// The minimum severity of the messages that the sinks receive. The default is
// RTCLoggingSeverityInfo.
@property(nonatomic, assign) RTCLoggingSeverity minSeverity;

// Whether log lines include the thread ID. The default is NO.
@property(nonatomic, assign) BOOL logThreads;

// Whether log lines include the elapsed time. The default is NO.
@property(nonatomic, assign) BOOL logTimestamps;

// Adds a sink that receives the log messages. The sink is kept for the
// lifetime of the process and can't be removed. The handler is called on the
// thread that logs, without synchronization, so it must be thread-safe. If it
// can be slow, it should dispatch to another queue.
- (void)addSinkWithHandler:(RTCLogSinkHandler)handler;

@end

// Initializes logging with `config`. Corresponds to
// webrtc::InitializeLogging().
// Only the first call has an effect. Returns NO if logging has already been
// initialized, either by an earlier call or because WebRTC has already logged.
// Call this before any other call into WebRTC.
RTC_EXTERN BOOL RTCInitializeLogging(RTC_OBJC_TYPE(RTCLoggingConfig) * config);

NS_ASSUME_NONNULL_END
