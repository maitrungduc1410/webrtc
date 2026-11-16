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
#import <XCTest/XCTest.h>

#import "api/logging/RTCLoggingConfig.h"
#import "base/RTCLogging.h"

#include <utility>

#include "rtc_base/logging.h"

@interface RTCLoggingConfigTest : XCTestCase
@end

@implementation RTCLoggingConfigTest {
  webrtc::LoggingConfig _savedConfig;
  NSMutableArray<NSString *> *_messages;
  NSMutableArray<NSNumber *> *_severities;
}

- (void)setUp {
  [super setUp];
  // Logging is configured once per process, so reset it to let each test call
  // RTCInitializeLogging().
  _savedConfig = webrtc::ResetLoggingForTesting();
  _messages = [NSMutableArray array];
  _severities = [NSMutableArray array];
}

- (void)tearDown {
  webrtc::ResetLoggingForTesting(std::move(_savedConfig));
  [super tearDown];
}

// Returns a config with a sink that records the messages that it receives.
- (RTC_OBJC_TYPE(RTCLoggingConfig) *)configWithRecordingSink {
  RTC_OBJC_TYPE(RTCLoggingConfig) *config =
      [[RTC_OBJC_TYPE(RTCLoggingConfig) alloc] init];
  NSMutableArray<NSString *> *messages = _messages;
  NSMutableArray<NSNumber *> *severities = _severities;
  [config addSinkWithHandler:^(NSString *message, RTCLoggingSeverity severity) {
    // The sink is called on the thread that logs.
    @synchronized(messages) {
      [messages addObject:message];
      [severities addObject:@(severity)];
    }
  }];
  return config;
}

// Returns whether the recording sink received a message that ends with
// `suffix` and has the given severity.
- (BOOL)hasMessageWithSuffix:(NSString *)suffix
                    severity:(RTCLoggingSeverity)severity {
  @synchronized(_messages) {
    for (NSUInteger i = 0; i < _messages.count; ++i) {
      if ([_messages[i] hasSuffix:suffix] &&
          _severities[i].integerValue == severity) {
        return YES;
      }
    }
  }
  return NO;
}

- (void)testDefaultsMatchNativeConfig {
  RTC_OBJC_TYPE(RTCLoggingConfig) *config =
      [[RTC_OBJC_TYPE(RTCLoggingConfig) alloc] init];

#if RTC_DLOG_IS_ON
  XCTAssertEqual(config.debugSeverity, RTCLoggingSeverityInfo);
#else
  XCTAssertEqual(config.debugSeverity, RTCLoggingSeverityNone);
#endif
  XCTAssertEqual(config.minSeverity, RTCLoggingSeverityInfo);
  XCTAssertFalse(config.logThreads);
  XCTAssertFalse(config.logTimestamps);
}

- (void)testOptionsAreAppliedToNativeConfig {
  RTC_OBJC_TYPE(RTCLoggingConfig) *config =
      [[RTC_OBJC_TYPE(RTCLoggingConfig) alloc] init];
  config.debugSeverity = RTCLoggingSeverityWarning;
  config.minSeverity = RTCLoggingSeverityError;
  config.logThreads = YES;
  config.logTimestamps = YES;

  XCTAssertTrue(RTCInitializeLogging(config));

  const webrtc::LoggingConfig &nativeConfig = webrtc::GetLoggingConfig();
  XCTAssertEqual(nativeConfig.debug_severity(), webrtc::LS_WARNING);
  XCTAssertEqual(nativeConfig.min_severity(), webrtc::LS_ERROR);
  XCTAssertTrue(nativeConfig.log_thread());
  XCTAssertTrue(nativeConfig.log_timestamp());
}

- (void)testSinkReceivesMessageAndSeverity {
  XCTAssertTrue(RTCInitializeLogging([self configWithRecordingSink]));

  RTCLogError(@"Horrible error");

  XCTAssertTrue([self hasMessageWithSuffix:@"Horrible error\n"
                                  severity:RTCLoggingSeverityError]);
}

- (void)testSinkDoesNotReceiveMessagesBelowMinSeverity {
  RTC_OBJC_TYPE(RTCLoggingConfig) *config = [self configWithRecordingSink];
  config.minSeverity = RTCLoggingSeverityWarning;
  XCTAssertTrue(RTCInitializeLogging(config));

  RTCLogInfo(@"Just some info");
  RTCLogWarning(@"Warning warning");

  XCTAssertFalse([self hasMessageWithSuffix:@"Just some info\n"
                                   severity:RTCLoggingSeverityInfo]);
  XCTAssertTrue([self hasMessageWithSuffix:@"Warning warning\n"
                                  severity:RTCLoggingSeverityWarning]);
}

- (void)testSinkReceivesMessageThatIsNotValidUtf8 {
  XCTAssertTrue(RTCInitializeLogging([self configWithRecordingSink]));

  RTC_LOG(LS_ERROR) << "Invalid UTF-8: \xff";

  // The line is decoded as Latin-1.
  XCTAssertTrue([self hasMessageWithSuffix:@"Invalid UTF-8: \u00ff\n"
                                  severity:RTCLoggingSeverityError]);
}

- (void)testOnlyFirstConfigIsApplied {
  RTC_OBJC_TYPE(RTCLoggingConfig) *firstConfig =
      [[RTC_OBJC_TYPE(RTCLoggingConfig) alloc] init];
  XCTAssertTrue(RTCInitializeLogging(firstConfig));

  XCTAssertFalse(RTCInitializeLogging([self configWithRecordingSink]));
  RTCLogError(@"Horrible error");

  XCTAssertFalse([self hasMessageWithSuffix:@"Horrible error\n"
                                   severity:RTCLoggingSeverityError]);
}

@end
