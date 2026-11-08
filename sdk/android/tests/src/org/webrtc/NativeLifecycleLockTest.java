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
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;
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

  @Test
  public void testDisposeOnTeardownThreadWaitsForDisposer() {
    AtomicLong disposedPtr = new AtomicLong(0);
    AtomicReference<Thread> disposerThread = new AtomicReference<>();
    lifecycleLock.disposeOnTeardownThread(
        ptr -> {
          disposedPtr.set(ptr);
          disposerThread.set(Thread.currentThread());
        },
        /* waitForCompletion= */ true);
    assertThat(disposedPtr.get()).isEqualTo(FAKE_PTR);
    assertThat(disposerThread.get()).isNotSameInstanceAs(Thread.currentThread());
    assertThat(lifecycleLock.isDisposed()).isTrue();
  }

  @Test
  public void testDisposeOnTeardownThreadIsIdempotent() {
    AtomicInteger disposeCount = new AtomicInteger(0);
    lifecycleLock.disposeOnTeardownThread(
        ptr -> disposeCount.incrementAndGet(), /* waitForCompletion= */ true);
    lifecycleLock.disposeOnTeardownThread(
        ptr -> disposeCount.incrementAndGet(), /* waitForCompletion= */ true);
    assertThat(disposeCount.get()).isEqualTo(1);
  }

  @Test
  public void testDisposeOnTeardownThreadWaitsForInFlightCall() throws Exception {
    CountDownLatch inFlightStarted = new CountDownLatch(1);
    CountDownLatch allowInFlightToComplete = new CountDownLatch(1);
    CountDownLatch disposed = new CountDownLatch(1);
    Thread reader =
        new Thread(
            () ->
                lifecycleLock.run(
                    () -> {
                      inFlightStarted.countDown();
                      ThreadUtils.awaitUninterruptibly(allowInFlightToComplete);
                    }));
    reader.start();
    assertThat(inFlightStarted.await(5, TimeUnit.SECONDS)).isTrue();

    lifecycleLock.disposeOnTeardownThread(
        ptr -> disposed.countDown(), /* waitForCompletion= */ false);
    assertThat(disposed.await(100, TimeUnit.MILLISECONDS)).isFalse();

    allowInFlightToComplete.countDown();
    assertThat(disposed.await(5, TimeUnit.SECONDS)).isTrue();
    reader.join(5000);
  }

  @Test
  public void testDisposeOnTeardownThreadWaitsForPendingDisposer() throws Exception {
    CountDownLatch disposerStarted = new CountDownLatch(1);
    CountDownLatch releaseDisposer = new CountDownLatch(1);
    AtomicInteger disposeCount = new AtomicInteger(0);
    lifecycleLock.disposeOnTeardownThread(
        ptr -> {
          disposerStarted.countDown();
          ThreadUtils.awaitUninterruptibly(releaseDisposer);
          disposeCount.incrementAndGet();
        },
        /* waitForCompletion= */ false);
    CountDownLatch secondRequestReturned = new CountDownLatch(1);
    Thread waiter =
        new Thread(
            () -> {
              lifecycleLock.disposeOnTeardownThread(
                  ptr -> disposeCount.incrementAndGet(), /* waitForCompletion= */ true);
              secondRequestReturned.countDown();
            });
    try {
      assertThat(disposerStarted.await(5, TimeUnit.SECONDS)).isTrue();
      assertThat(lifecycleLock.isDisposed()).isTrue();
      waiter.start();
      // A request that waits must not return before the pending disposer has finished.
      assertThat(secondRequestReturned.await(100, TimeUnit.MILLISECONDS)).isFalse();
    } finally {
      // Unblocks the shared teardown thread even if an assertion failed, so that later tests
      // don't hang.
      releaseDisposer.countDown();
    }
    assertThat(secondRequestReturned.await(5, TimeUnit.SECONDS)).isTrue();
    assertThat(disposeCount.get()).isEqualTo(1);
    waiter.join(5000);
  }

  @Test
  public void testDisposeOnTeardownThreadDoesNotWaitAfterCompletion() throws Exception {
    lifecycleLock.disposeOnTeardownThread(ptr -> {}, /* waitForCompletion= */ true);

    // Occupy the shared teardown thread with an unrelated disposer.
    NativeLifecycleLock otherLock = new NativeLifecycleLock("OtherClass", FAKE_PTR);
    CountDownLatch otherDisposerStarted = new CountDownLatch(1);
    CountDownLatch releaseOtherDisposer = new CountDownLatch(1);
    otherLock.disposeOnTeardownThread(
        ptr -> {
          otherDisposerStarted.countDown();
          ThreadUtils.awaitUninterruptibly(releaseOtherDisposer);
        },
        /* waitForCompletion= */ false);
    CountDownLatch repeatedRequestReturned = new CountDownLatch(1);
    Thread repeater =
        new Thread(
            () -> {
              lifecycleLock.disposeOnTeardownThread(ptr -> {}, /* waitForCompletion= */ true);
              repeatedRequestReturned.countDown();
            });
    try {
      assertThat(otherDisposerStarted.await(5, TimeUnit.SECONDS)).isTrue();
      repeater.start();
      // A repeated request returns without waiting for the unrelated disposer.
      assertThat(repeatedRequestReturned.await(5, TimeUnit.SECONDS)).isTrue();
    } finally {
      // Unblocks the shared teardown thread even if an assertion failed, so that later tests
      // don't hang.
      releaseOtherDisposer.countDown();
    }
    repeater.join(5000);
  }

  @Test
  public void testDisposeOnTeardownThreadDoesNotWaitInsideCall() throws Exception {
    CountDownLatch disposed = new CountDownLatch(1);
    lifecycleLock.run(
        () -> {
          lifecycleLock.disposeOnTeardownThread(
              ptr -> disposed.countDown(), /* waitForCompletion= */ true);
          // The disposer can't run until this call has finished.
          assertThat(disposed.getCount()).isEqualTo(1);
        });
    assertThat(disposed.await(5, TimeUnit.SECONDS)).isTrue();
  }

  @Test
  public void testDisposerCanCallIntoDisposedObject() {
    AtomicReference<String> result = new AtomicReference<>();
    lifecycleLock.disposeOnTeardownThread(
        ptr -> result.set(lifecycleLock.callOrDefault(() -> "alive", "disposed")),
        /* waitForCompletion= */ true);
    assertThat(result.get()).isEqualTo("disposed");
  }

  @Test
  public void testDisposeOnTeardownThreadRunsDisposersInOrder() throws Exception {
    NativeLifecycleLock otherLock = new NativeLifecycleLock("OtherClass", FAKE_PTR);
    List<String> order = Collections.synchronizedList(new ArrayList<>());
    CountDownLatch firstStarted = new CountDownLatch(1);
    CountDownLatch releaseFirst = new CountDownLatch(1);
    CountDownLatch secondDone = new CountDownLatch(1);
    lifecycleLock.disposeOnTeardownThread(
        ptr -> {
          firstStarted.countDown();
          ThreadUtils.awaitUninterruptibly(releaseFirst);
          order.add("first");
        },
        /* waitForCompletion= */ false);
    assertThat(firstStarted.await(5, TimeUnit.SECONDS)).isTrue();
    otherLock.disposeOnTeardownThread(
        ptr -> {
          order.add("second");
          secondDone.countDown();
        },
        /* waitForCompletion= */ false);

    releaseFirst.countDown();
    assertThat(secondDone.await(5, TimeUnit.SECONDS)).isTrue();
    assertThat(order).containsExactly("first", "second").inOrder();
  }
}
