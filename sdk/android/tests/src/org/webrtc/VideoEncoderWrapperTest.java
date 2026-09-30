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
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import androidx.test.runner.AndroidJUnit4;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

@RunWith(AndroidJUnit4.class)
@Config(manifest = Config.NONE)
public class VideoEncoderWrapperTest {
  private static final long FAKE_NATIVE_ENCODER = 0x12345678L;
  private static final long TIMEOUT_MS = 5000;

  @Mock private VideoEncoderWrapper.Natives mockNatives;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);
    VideoEncoderWrapperJni.setInstanceForTesting(mockNatives);
  }

  @After
  public void tearDown() {
    VideoEncoderWrapperJni.setInstanceForTesting(null);
  }

  @Test
  public void testCallbackForwardsFramesToNative() {
    VideoEncoderWrapper.NativeEncoderCallback callback =
        VideoEncoderWrapper.createEncoderCallback(FAKE_NATIVE_ENCODER);

    callback.onEncodedFrame(/* frame= */ null, new VideoEncoder.CodecSpecificInfo());

    verify(mockNatives).onEncodedFrame(eq(FAKE_NATIVE_ENCODER), isNull());
  }

  @Test
  public void testCallbackDropsFramesAfterInvalidate() {
    VideoEncoderWrapper.NativeEncoderCallback callback =
        VideoEncoderWrapper.createEncoderCallback(FAKE_NATIVE_ENCODER);

    callback.invalidate();
    callback.onEncodedFrame(/* frame= */ null, new VideoEncoder.CodecSpecificInfo());

    verifyNoInteractions(mockNatives);
  }

  @Test
  public void testInvalidateWaitsForOngoingCallback() throws InterruptedException {
    final CountDownLatch nativeEntered = new CountDownLatch(1);
    final CountDownLatch allowNativeReturn = new CountDownLatch(1);
    doAnswer(invocation -> {
      nativeEntered.countDown();
      allowNativeReturn.await();
      return null;
    })
        .when(mockNatives)
        .onEncodedFrame(anyLong(), isNull());
    VideoEncoderWrapper.NativeEncoderCallback callback =
        VideoEncoderWrapper.createEncoderCallback(FAKE_NATIVE_ENCODER);

    Thread deliveryThread = new Thread(
        () -> callback.onEncodedFrame(/* frame= */ null, new VideoEncoder.CodecSpecificInfo()));
    deliveryThread.start();
    assertThat(nativeEntered.await(TIMEOUT_MS, TimeUnit.MILLISECONDS)).isTrue();

    final CountDownLatch invalidated = new CountDownLatch(1);
    Thread invalidateThread = new Thread(() -> {
      callback.invalidate();
      invalidated.countDown();
    });
    invalidateThread.start();

    // invalidate() must not return while the native callback is running.
    assertThat(invalidated.await(100, TimeUnit.MILLISECONDS)).isFalse();

    allowNativeReturn.countDown();
    assertThat(invalidated.await(TIMEOUT_MS, TimeUnit.MILLISECONDS)).isTrue();
    deliveryThread.join(TIMEOUT_MS);
    invalidateThread.join(TIMEOUT_MS);
  }
}
