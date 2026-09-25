/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import androidx.annotation.Nullable;
import org.jni_zero.NativeMethods;

/** Java wrapper for a C++ RtpReceiverInterface. */
public class RtpReceiver {
  /** Java wrapper for a C++ RtpReceiverObserverInterface*/
  public static interface Observer {
    // Called when the first audio or video packet is received.
    @CalledByNative
    public void onFirstPacketReceived(MediaStreamTrack.MediaType media_type);
    // Called when the first audio or video packet is received after
    // receptiveness changed.
    // TODO: crbug.com/40821064 - remove default implementation.
    @CalledByNative
    public default void onFirstPacketReceivedAfterReceptiveChange(
      MediaStreamTrack.MediaType media_type) {}
  }

  final NativeLifecycleLock lifecycleLock;
  private long nativeObserver;

  @Nullable private final MediaStreamTrack cachedTrack;

  @CalledByNative
  public RtpReceiver(long nativeRtpReceiver) {
    this.lifecycleLock = new NativeLifecycleLock("RtpReceiver", nativeRtpReceiver);
    long nativeTrack = RtpReceiverJni.get().getTrack(nativeRtpReceiver);
    cachedTrack = MediaStreamTrack.createMediaStreamTrack(nativeTrack);
  }

  @Nullable
  public MediaStreamTrack track() {
    return cachedTrack;
  }

  public RtpParameters getParameters() {
    return lifecycleLock.call(receiver -> RtpReceiverJni.get().getParameters(receiver));
  }

  public String id() {
    return lifecycleLock.call(receiver -> RtpReceiverJni.get().getId(receiver));
  }

  /** Returns a pointer to webrtc::RtpReceiverInterface. */
  long getNativeRtpReceiver() {
    return lifecycleLock.getNativePointer();
  }

  @CalledByNative
  public void dispose() {
    lifecycleLock.dispose(
        receiver -> {
          cachedTrack.dispose();
          if (nativeObserver != 0) {
            RtpReceiverJni.get().unsetObserver(receiver, nativeObserver);
            nativeObserver = 0;
          }
          JniCommon.nativeReleaseRef(receiver);
        });
  }

  public void SetObserver(Observer observer) {
    if (observer == null) {
      lifecycleLock.runIfAlive(
          receiver -> {
            if (nativeObserver != 0) {
              RtpReceiverJni.get().unsetObserver(receiver, nativeObserver);
              nativeObserver = 0;
            }
          });
      return;
    }
    lifecycleLock.run(
        receiver -> {
          // Unset the existing one before setting a new one.
          if (nativeObserver != 0) {
            RtpReceiverJni.get().unsetObserver(receiver, nativeObserver);
          }
          nativeObserver = RtpReceiverJni.get().setObserver(receiver, observer);
        });
  }

  public void setFrameDecryptor(FrameDecryptor frameDecryptor) {
    lifecycleLock.run(
        receiver ->
            RtpReceiverJni.get()
                .setFrameDecryptor(receiver, frameDecryptor.getNativeFrameDecryptor()));
  }

  @NativeMethods
  interface Natives {
    long getTrack(long rtpReceiver);

    RtpParameters getParameters(long rtpReceiver);

    String getId(long rtpReceiver);

    long setObserver(long rtpReceiver, Observer observer);

    void unsetObserver(long rtpReceiver, long nativeObserver);

    void setFrameDecryptor(long rtpReceiver, long nativeFrameDecryptor);
  }
}
