/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCLoggingConfig.h"

#import "helpers/NSString+StdString.h"

#include <memory>
#include <string>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace {

webrtc::LoggingSeverity ObjCToNativeSeverity(RTCLoggingSeverity severity) {
  switch (severity) {
    case RTCLoggingSeverityVerbose:
      return webrtc::LS_VERBOSE;
    case RTCLoggingSeverityInfo:
      return webrtc::LS_INFO;
    case RTCLoggingSeverityWarning:
      return webrtc::LS_WARNING;
    case RTCLoggingSeverityError:
      return webrtc::LS_ERROR;
    case RTCLoggingSeverityNone:
      return webrtc::LS_NONE;
  }
  // Callers can pass any integer. There is no default case, so that the
  // compiler still warns about enumerators that aren't handled.
  RTC_CHECK_NOTREACHED();
}

RTCLoggingSeverity NativeToObjCSeverity(webrtc::LoggingSeverity severity) {
  switch (severity) {
    case webrtc::LS_VERBOSE:
      return RTCLoggingSeverityVerbose;
    case webrtc::LS_INFO:
      return RTCLoggingSeverityInfo;
    case webrtc::LS_WARNING:
      return RTCLoggingSeverityWarning;
    case webrtc::LS_ERROR:
      return RTCLoggingSeverityError;
    case webrtc::LS_NONE:
      return RTCLoggingSeverityNone;
  }
}

// Passes log messages to a handler that was added with -addSinkWithHandler:.
class HandlerLogSink final : public webrtc::LogSink {
 public:
  explicit HandlerLogSink(RTCLogSinkHandler handler) : handler_(handler) {}

  void OnLogMessage(const webrtc::LogLineRef& line) override {
    // Called on the thread that logs, which may not have an autorelease pool.
    @autoreleasepool {
      const std::string& text = line.DefaultLogLine();
      NSString* message = [NSString stringForStdString:text];
      if (message == nil) {
        // The line isn't valid UTF-8, for example because it contains bytes
        // from the network, but the handler takes a nonnull message. Any bytes
        // are valid Latin-1, which keeps the ASCII parts readable.
        message = [[NSString alloc] initWithBytes:text.data()
                                           length:text.size()
                                         encoding:NSISOLatin1StringEncoding];
      }
      handler_(message, NativeToObjCSeverity(line.severity()));
    }
  }

 private:
  const RTCLogSinkHandler handler_;
};

}  // namespace

@interface RTC_OBJC_TYPE (RTCLoggingConfig)
()

    // The handlers that were added with -addSinkWithHandler:.
    @property(nonatomic, readonly)
        NSMutableArray<RTCLogSinkHandler>* sinkHandlers;

@end

@implementation RTC_OBJC_TYPE (RTCLoggingConfig)

@synthesize debugSeverity = _debugSeverity;
@synthesize minSeverity = _minSeverity;
@synthesize logThreads = _logThreads;
@synthesize logTimestamps = _logTimestamps;
@synthesize sinkHandlers = _sinkHandlers;

- (instancetype)init {
  self = [super init];
  if (self) {
    // webrtc::LoggingConfig provides the default values.
    const webrtc::LoggingConfig defaults;
    _debugSeverity = NativeToObjCSeverity(defaults.debug_severity());
    _minSeverity = NativeToObjCSeverity(defaults.min_severity());
    _logThreads = defaults.log_thread();
    _logTimestamps = defaults.log_timestamp();
    _sinkHandlers = [NSMutableArray array];
  }
  return self;
}

- (void)addSinkWithHandler:(RTCLogSinkHandler)handler {
  RTC_DCHECK(handler != nil);
  [_sinkHandlers addObject:[handler copy]];
}

@end

BOOL RTCInitializeLogging(RTC_OBJC_TYPE(RTCLoggingConfig) * config) {
  // Not a DCHECK: the properties of a nil config read as 0, which would turn on
  // verbose logging in release builds.
  RTC_CHECK(config != nil);
  webrtc::LoggingConfig native_config;
  native_config.set_debug_severity(ObjCToNativeSeverity(config.debugSeverity));
  native_config.set_min_severity(ObjCToNativeSeverity(config.minSeverity));
  native_config.set_log_thread(config.logThreads);
  native_config.set_log_timestamp(config.logTimestamps);
  for (RTCLogSinkHandler handler in config.sinkHandlers) {
    native_config.AddSink(std::make_unique<HandlerLogSink>(handler));
  }
  return webrtc::InitializeLogging(std::move(native_config));
}
