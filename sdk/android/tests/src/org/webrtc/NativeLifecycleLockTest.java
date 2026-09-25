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
import static org.junit.Assert.fail;

import androidx.test.runner.AndroidJUnit4;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

@RunWith(AndroidJUnit4.class)
@Config(manifest = Config.NONE)
public class NativeLifecycleLockTest {
  private static final long FAKE_PTR = 0x12345678L;
  private NativeLifecycleLock lifecycleLock;

  @Before
  public void setUp() {
    lifecycleLock = new NativeLifecycleLock("TestClass", FAKE_PTR);
  }

  @Test
  public void testCallWithPointerReturnsValue() {
    String result =
        lifecycleLock.call(
            ptr -> {
              assertThat(ptr).isEqualTo(FAKE_PTR);
              return "success";
            });
    assertThat(result).isEqualTo("success");
  }

  @Test
  public void testCallNoArgReturnsValue() {
    String result = lifecycleLock.call(() -> "success");
    assertThat(result).isEqualTo("success");
  }

  @Test
  public void testCallOrDefaultReturnsValueWhenAlive() {
    assertThat(lifecycleLock.callOrDefault(() -> "alive", "default")).isEqualTo("alive");
    assertThat(lifecycleLock.callOrDefault(ptr -> "ptr:" + ptr, "default"))
        .isEqualTo("ptr:" + FAKE_PTR);
  }

  @Test
  public void testCallOrDefaultReturnsDefaultAfterDispose() {
    lifecycleLock.dispose(ptr -> {});
    assertThat(lifecycleLock.callOrDefault(() -> "alive", "default")).isEqualTo("default");
    assertThat(lifecycleLock.callOrDefault(ptr -> "alive", "default")).isEqualTo("default");
  }

  @Test
  public void testRunWithPointerExecutes() {
    AtomicLong seenPtr = new AtomicLong(0);
    lifecycleLock.run(seenPtr::set);
    assertThat(seenPtr.get()).isEqualTo(FAKE_PTR);
  }

  @Test
  public void testRunNoArgExecutes() {
    AtomicBoolean executed = new AtomicBoolean(false);
    lifecycleLock.run(() -> executed.set(true));
    assertThat(executed.get()).isTrue();
  }

  @Test
  public void testRunIfAliveExecutesWhenAlive() {
    AtomicBoolean noArgExecuted = new AtomicBoolean(false);
    AtomicLong seenPtr = new AtomicLong(0);

    assertThat(lifecycleLock.runIfAlive(() -> noArgExecuted.set(true))).isTrue();
    assertThat(lifecycleLock.runIfAlive(seenPtr::set)).isTrue();
    assertThat(noArgExecuted.get()).isTrue();
    assertThat(seenPtr.get()).isEqualTo(FAKE_PTR);
  }

  @Test
  public void testRunIfAliveReturnsFalseAfterDispose() {
    lifecycleLock.dispose(ptr -> {});
    AtomicBoolean executed = new AtomicBoolean(false);

    assertThat(lifecycleLock.runIfAlive(() -> executed.set(true))).isFalse();
    assertThat(lifecycleLock.runIfAlive(ptr -> executed.set(true))).isFalse();
    assertThat(executed.get()).isFalse();
  }

  @Test
  public void testGetNativePointer() {
    assertThat(lifecycleLock.getNativePointer()).isEqualTo(FAKE_PTR);
  }

  @Test
  public void testIsDisposedInitiallyFalse() {
    assertThat(lifecycleLock.isDisposed()).isFalse();
  }

  @Test
  public void testDisposeExecutesDisposerAndSetsDisposed() {
    AtomicLong disposedPtr = new AtomicLong(0);
    lifecycleLock.dispose(disposedPtr::set);

    assertThat(disposedPtr.get()).isEqualTo(FAKE_PTR);
    assertThat(lifecycleLock.isDisposed()).isTrue();
  }

  @Test
  public void testDisposeIsIdempotent() {
    AtomicInteger callCount = new AtomicInteger(0);
    lifecycleLock.dispose(ptr -> callCount.incrementAndGet());
    lifecycleLock.dispose(ptr -> callCount.incrementAndGet());

    assertThat(callCount.get()).isEqualTo(1);
    assertThat(lifecycleLock.isDisposed()).isTrue();
  }

  @Test(expected = IllegalStateException.class)
  public void testCallWithPointerAfterDisposeThrowsIllegalStateException() {
    lifecycleLock.dispose(ptr -> {});
    lifecycleLock.call(ptr -> "failed");
  }

  @Test(expected = IllegalStateException.class)
  public void testCallNoArgAfterDisposeThrowsIllegalStateException() {
    lifecycleLock.dispose(ptr -> {});
    lifecycleLock.call(() -> "failed");
  }

  @Test(expected = IllegalStateException.class)
  public void testRunWithPointerAfterDisposeThrowsIllegalStateException() {
    lifecycleLock.dispose(ptr -> {});
    lifecycleLock.run(ptr -> {});
  }

  @Test(expected = IllegalStateException.class)
  public void testRunNoArgAfterDisposeThrowsIllegalStateException() {
    lifecycleLock.dispose(ptr -> {});
    lifecycleLock.run(() -> {});
  }

  @Test(expected = IllegalStateException.class)
  public void testGetNativePointerAfterDisposeThrowsIllegalStateException() {
    lifecycleLock.dispose(ptr -> {});
    lifecycleLock.getNativePointer();
  }

  @Test
  public void testExceptionMessageContainsClassName() {
    lifecycleLock.dispose(ptr -> {});
    try {
      lifecycleLock.call(ptr -> "failed");
      fail("Expected IllegalStateException");
    } catch (IllegalStateException e) {
      assertThat(e).hasMessageThat().contains("TestClass has been disposed.");
    }
  }

  @Test
  public void testDisposeBlocksUntilInFlightCallFinishes() throws Exception {
    CountDownLatch inFlightStarted = new CountDownLatch(1);
    CountDownLatch allowInFlightToComplete = new CountDownLatch(1);
    CountDownLatch disposeFinished = new CountDownLatch(1);
    AtomicBoolean callCompleted = new AtomicBoolean(false);

    Thread threadA =
        new Thread(
            () -> {
              lifecycleLock.run(
                  () -> {
                    inFlightStarted.countDown();
                    try {
                      allowInFlightToComplete.await(5, TimeUnit.SECONDS);
                      callCompleted.set(true);
                    } catch (InterruptedException e) {
                      Thread.currentThread().interrupt();
                    }
                  });
            });
    threadA.start();

    assertThat(inFlightStarted.await(5, TimeUnit.SECONDS)).isTrue();

    Thread threadB =
        new Thread(
            () -> {
              lifecycleLock.dispose(ptr -> {});
              disposeFinished.countDown();
            });
    threadB.start();

    // Verify dispose does not finish while in-flight read lock is held.
    assertThat(disposeFinished.await(100, TimeUnit.MILLISECONDS)).isFalse();
    assertThat(callCompleted.get()).isFalse();

    allowInFlightToComplete.countDown();
    threadA.join(5000);
    threadB.join(5000);

    assertThat(callCompleted.get()).isTrue();
    assertThat(disposeFinished.getCount()).isEqualTo(0);
    assertThat(lifecycleLock.isDisposed()).isTrue();
  }
}
