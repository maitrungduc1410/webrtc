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

import androidx.annotation.Nullable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.ReentrantReadWriteLock;

/**
 * Thread-safe lifecycle lock for WebRTC native object wrappers. Guards native pointer access with a
 * read-lock and destruction with a write-lock, preventing Use-After-Free race conditions between
 * JNI method calls and dispose().
 */
class NativeLifecycleLock {
  public interface NativeCallable<T> {
    T call();
  }

  public interface NativePointerCallable<T> {
    T call(long nativePointer);
  }

  public interface NativeRunnable {
    void run();
  }

  public interface NativePointerRunnable {
    void run(long nativePointer);
  }

  public interface NativeDisposer {
    void dispose(long nativePointer);
  }

  // Runs disposeOnTeardownThread() requests in order on a thread that exits when idle.
  private static final ThreadPoolExecutor teardownExecutor = createTeardownExecutor();

  private final ReentrantReadWriteLock lock = new ReentrantReadWriteLock();
  private final String className;
  private long nativePointer;
  // Set once the disposer has returned. disposeOnTeardownThread() clears nativePointer before it
  // runs the disposer.
  private volatile boolean disposeCompleted;

  NativeLifecycleLock(String className, long nativePointer) {
    this.className = className;
    this.nativePointer = nativePointer;
    this.disposeCompleted = nativePointer == 0;
  }

  <T> T call(NativeCallable<T> callable) {
    lock.readLock().lock();
    try {
      checkNotDisposed();
      return callable.call();
    } finally {
      lock.readLock().unlock();
    }
  }

  <T> T call(NativePointerCallable<T> callable) {
    lock.readLock().lock();
    try {
      checkNotDisposed();
      return callable.call(nativePointer);
    } finally {
      lock.readLock().unlock();
    }
  }

  @Nullable
  <T> T callOrDefault(NativeCallable<T> callable, @Nullable T defaultValue) {
    lock.readLock().lock();
    try {
      if (nativePointer == 0) {
        Logging.w(className, className + " has been disposed.");
        return defaultValue;
      }
      return callable.call();
    } finally {
      lock.readLock().unlock();
    }
  }

  @Nullable
  <T> T callOrDefault(NativePointerCallable<T> callable, @Nullable T defaultValue) {
    lock.readLock().lock();
    try {
      if (nativePointer == 0) {
        Logging.w(className, className + " has been disposed.");
        return defaultValue;
      }
      return callable.call(nativePointer);
    } finally {
      lock.readLock().unlock();
    }
  }

  void run(NativeRunnable runnable) {
    lock.readLock().lock();
    try {
      checkNotDisposed();
      runnable.run();
    } finally {
      lock.readLock().unlock();
    }
  }

  void run(NativePointerRunnable runnable) {
    lock.readLock().lock();
    try {
      checkNotDisposed();
      runnable.run(nativePointer);
    } finally {
      lock.readLock().unlock();
    }
  }

  boolean runIfAlive(NativeRunnable runnable) {
    lock.readLock().lock();
    try {
      if (nativePointer == 0) {
        Logging.w(className, className + " has been disposed.");
        return false;
      }
      runnable.run();
      return true;
    } finally {
      lock.readLock().unlock();
    }
  }

  boolean runIfAlive(NativePointerRunnable runnable) {
    lock.readLock().lock();
    try {
      if (nativePointer == 0) {
        Logging.w(className, className + " has been disposed.");
        return false;
      }
      runnable.run(nativePointer);
      return true;
    } finally {
      lock.readLock().unlock();
    }
  }

  boolean isDisposed() {
    lock.readLock().lock();
    try {
      return nativePointer == 0;
    } finally {
      lock.readLock().unlock();
    }
  }

  void dispose(NativeDisposer disposer) {
    if (isDisposed()) {
      return;
    }
    lock.writeLock().lock();
    try {
      if (nativePointer != 0) {
        disposer.dispose(nativePointer);
        nativePointer = 0;
        disposeCompleted = true;
      }
    } finally {
      lock.writeLock().unlock();
    }
  }

  /**
   * Disposes the native object on a shared teardown thread, which handles requests in order.
   *
   * <p>On that thread, this waits for in-flight calls to finish, marks this lock as disposed and
   * then calls {@code disposer} without holding the lock. The disposer may therefore block on
   * WebRTC threads and trigger callbacks that call back into the object.
   *
   * @param waitForCompletion Whether to block until the native object has been disposed, including
   *     when an earlier request is still disposing it. Must be false on threads that in-flight
   *     calls or native teardown may block on, such as WebRTC threads, and in disposers. Ignored if
   *     the current thread holds the read lock, since waiting would then deadlock.
   */
  void disposeOnTeardownThread(NativeDisposer disposer, boolean waitForCompletion) {
    boolean wait = waitForCompletion && lock.getReadHoldCount() == 0;
    // isDisposed() is already true while the disposer runs. A request that waits is then queued
    // anyway; requests run in order, so it completes once the disposer has returned.
    if (disposeCompleted || (!wait && isDisposed())) {
      return;
    }
    CountDownLatch disposed = new CountDownLatch(1);
    teardownExecutor.execute(
        () -> {
          try {
            long ptr = detach();
            if (ptr != 0) {
              disposer.dispose(ptr);
            }
          } finally {
            disposeCompleted = true;
            disposed.countDown();
          }
        });
    if (wait) {
      ThreadUtils.awaitUninterruptibly(disposed);
    }
  }

  long getNativePointer() {
    lock.readLock().lock();
    try {
      checkNotDisposed();
      return nativePointer;
    } finally {
      lock.readLock().unlock();
    }
  }

  private void checkNotDisposed() {
    if (nativePointer == 0) {
      throw new IllegalStateException(className + " has been disposed.");
    }
  }

  // Waits for in-flight calls to finish, marks this lock as disposed and returns the native
  // pointer, or 0 if already disposed.
  private long detach() {
    lock.writeLock().lock();
    try {
      long ptr = nativePointer;
      nativePointer = 0;
      return ptr;
    } finally {
      lock.writeLock().unlock();
    }
  }

  private static ThreadPoolExecutor createTeardownExecutor() {
    ThreadPoolExecutor executor =
        new ThreadPoolExecutor(
            /* corePoolSize= */ 1,
            /* maximumPoolSize= */ 1,
            /* keepAliveTime= */ 10,
            TimeUnit.SECONDS,
            new LinkedBlockingQueue<>(),
            runnable -> {
              Thread thread = new Thread(runnable, "WebRtcTeardown");
              thread.setDaemon(true);
              return thread;
            });
    executor.allowCoreThreadTimeOut(true);
    return executor;
  }
}
