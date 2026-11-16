/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import androidx.test.runner.AndroidJUnit4;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.webrtc.Logging.Severity;

@RunWith(AndroidJUnit4.class)
@Config(manifest = Config.NONE)
public class LoggingTest {
  private static final String TAG = "LoggingTest";

  @Mock private Logging.Natives mockNatives;
  @Mock private Loggable mockSink;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);
    LoggingJni.setInstanceForTesting(mockNatives);
    Logging.resetForTesting();
    when(mockNatives.initialize(any(), any(), anyBoolean(), anyBoolean(), any())).thenReturn(true);
  }

  @After
  public void tearDown() {
    Logging.resetForTesting();
    LoggingJni.setInstanceForTesting(null);
  }

  @Test
  public void testConfigIsAppliedWhenNativeLibraryIsLoaded() {
    assertThat(Logging.initialize(LoggingConfig.builder().build())).isTrue();
    verifyNoInteractions(mockNatives);

    Logging.onNativeLibraryLoaded();

    verify(mockNatives).initialize(any(), any(), anyBoolean(), anyBoolean(), any());
  }

  @Test
  public void testConfigIsAppliedImmediatelyIfNativeLibraryIsLoaded() {
    Logging.onNativeLibraryLoaded();
    verifyNoInteractions(mockNatives);

    assertThat(Logging.initialize(LoggingConfig.builder().build())).isTrue();

    verify(mockNatives).initialize(any(), any(), anyBoolean(), anyBoolean(), any());
  }

  @Test
  public void testOnlyFirstConfigIsApplied() {
    assertThat(Logging.initialize(LoggingConfig.builder().addSink(mockSink).build())).isTrue();
    assertThat(Logging.initialize(LoggingConfig.builder().build())).isFalse();

    Logging.onNativeLibraryLoaded();

    verify(mockNatives)
        .initialize(any(), any(), anyBoolean(), anyBoolean(), aryEq(new Loggable[] {mockSink}));
  }

  @Test
  public void testReturnsFalseIfNativeLoggingIsAlreadyInitialized() {
    when(mockNatives.initialize(any(), any(), anyBoolean(), anyBoolean(), any())).thenReturn(false);
    Logging.onNativeLibraryLoaded();

    assertThat(Logging.initialize(LoggingConfig.builder().build())).isFalse();
  }

  @Test
  public void testNullConfigIsRejectedWithoutUsingUpInitialization() {
    assertThrows(IllegalArgumentException.class, () -> Logging.initialize(null));

    assertThat(Logging.initialize(LoggingConfig.builder().build())).isTrue();
  }

  @Test
  public void testConfigIsAppliedOnceIfNativeLibraryLoadIsNotifiedTwice() {
    Logging.initialize(LoggingConfig.builder().build());

    // Native code notifies from JNI_OnLoad, and NativeLibrary notifies after loading.
    Logging.onNativeLibraryLoaded();
    Logging.onNativeLibraryLoaded();

    verify(mockNatives).initialize(any(), any(), anyBoolean(), anyBoolean(), any());
  }

  @Test
  public void testUnsetOptionsUseNativeDefaults() {
    Logging.onNativeLibraryLoaded();

    Logging.initialize(LoggingConfig.builder().build());

    verify(mockNatives)
        .initialize(isNull(), isNull(), eq(false), eq(false), aryEq(new Loggable[0]));
  }

  @Test
  @SuppressWarnings("EnumOrdinal")
  public void testOptionsArePassedToNative() {
    Logging.onNativeLibraryLoaded();

    Logging.initialize(LoggingConfig.builder()
                           .setDebugSeverity(Severity.LS_WARNING)
                           .setMinSeverity(Severity.LS_ERROR)
                           .setLogThreads(true)
                           .setLogTimestamps(true)
                           .addSink(mockSink)
                           .build());

    verify(mockNatives)
        .initialize(eq(Severity.LS_WARNING.ordinal()), eq(Severity.LS_ERROR.ordinal()), eq(true),
            eq(true), aryEq(new Loggable[] {mockSink}));
  }

  @Test
  @SuppressWarnings("EnumOrdinal")
  public void testJavaMessagesArePassedToNativeOnceNativeLibraryIsLoaded() {
    Logging.d(TAG, "Before");
    verify(mockNatives, never()).log(anyInt(), anyString(), anyString());

    // Without Logging.initialize(), since logging can also be initialized in native code.
    Logging.onNativeLibraryLoaded();
    Logging.d(TAG, "After");

    verify(mockNatives).log(Severity.LS_INFO.ordinal(), TAG, "After");
  }

  @Test
  @SuppressWarnings("EnumOrdinal")
  public void testJavaMessagesArePassedToNativeIfConfigIsIgnored() {
    when(mockNatives.initialize(any(), any(), anyBoolean(), anyBoolean(), any())).thenReturn(false);
    Logging.initialize(LoggingConfig.builder().build());

    // The warning about the ignored config isn't passed to native logging.
    Logging.onNativeLibraryLoaded();
    verify(mockNatives, never()).log(anyInt(), anyString(), anyString());

    Logging.d(TAG, "Message");
    verify(mockNatives).log(Severity.LS_INFO.ordinal(), TAG, "Message");
  }

  @Test
  @SuppressWarnings("EnumOrdinal")
  public void testNativeMessagesArePassedToSink() {
    Logging.onNativeLogMessage(mockSink, "Message", Severity.LS_WARNING.ordinal(), TAG);

    verify(mockSink).onLogMessage("Message", Severity.LS_WARNING, TAG);
  }
}
