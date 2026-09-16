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
import java.util.List;
import org.jni_zero.NativeMethods;

/** Java wrapper for a C++ RtpSenderInterface. */
public class RtpSender {
  private final NativeLifecycleLock lifecycleLock;

  @Nullable private MediaStreamTrack cachedTrack;
  private boolean ownsTrack = true;
  private final @Nullable DtmfSender dtmfSender;

  @CalledByNative
  public RtpSender(long nativeRtpSender) {
    this.lifecycleLock = new NativeLifecycleLock("RtpSender", nativeRtpSender);
    long nativeTrack = RtpSenderJni.get().getTrack(nativeRtpSender);
    cachedTrack = MediaStreamTrack.createMediaStreamTrack(nativeTrack);

    if (RtpSenderJni.get()
        .getMediaType(nativeRtpSender)
        .equalsIgnoreCase(MediaStreamTrack.AUDIO_TRACK_KIND)) {
      long nativeDtmfSender = RtpSenderJni.get().getDtmfSender(nativeRtpSender);
      dtmfSender = (nativeDtmfSender != 0) ? new DtmfSender(nativeDtmfSender) : null;
    } else {
      dtmfSender = null;
    }
  }

  /**
   * Starts sending a new track, without requiring additional SDP negotiation.
   *
   * <p>Note: This is equivalent to replaceTrack in the official WebRTC API. It was just implemented
   * before the standards group settled on a name.
   *
   * @param takeOwnership If true, the RtpSender takes ownership of the track from the caller, and
   *     will auto-dispose of it when no longer needed. `takeOwnership` should only be used if the
   *     caller owns the track; it is not appropriate when the track is owned by, for example,
   *     another RtpSender or a MediaStream.
   * @return true on success and false on failure.
   */
  public boolean setTrack(@Nullable MediaStreamTrack track, boolean takeOwnership) {
    return lifecycleLock.call(
        sender -> {
          if (!RtpSenderJni.get()
              .setTrack(sender, (track == null) ? 0 : track.getNativeMediaStreamTrack())) {
            return false;
          }
          if (cachedTrack != null && ownsTrack) {
            cachedTrack.dispose();
          }
          cachedTrack = track;
          ownsTrack = takeOwnership;
          return true;
        });
  }

  @Nullable
  public MediaStreamTrack track() {
    return cachedTrack;
  }

  public void setStreams(List<String> streamIds) {
    lifecycleLock.run(sender -> RtpSenderJni.get().setStreams(sender, streamIds));
  }

  public List<String> getStreams() {
    return lifecycleLock.call(sender -> RtpSenderJni.get().getStreams(sender));
  }

  public boolean setParameters(RtpParameters parameters) {
    return lifecycleLock.call(sender -> RtpSenderJni.get().setParameters(sender, parameters));
  }

  public RtpParameters getParameters() {
    return lifecycleLock.call(sender -> RtpSenderJni.get().getParameters(sender));
  }

  public String id() {
    return lifecycleLock.call(sender -> RtpSenderJni.get().getId(sender));
  }

  @Nullable
  public DtmfSender dtmf() {
    return dtmfSender;
  }

  public void setFrameEncryptor(FrameEncryptor frameEncryptor) {
    lifecycleLock.run(
        sender ->
            RtpSenderJni.get().setFrameEncryptor(sender, frameEncryptor.getNativeFrameEncryptor()));
  }

  public void dispose() {
    lifecycleLock.dispose(
        sender -> {
          if (dtmfSender != null) {
            dtmfSender.dispose();
          }
          if (cachedTrack != null && ownsTrack) {
            cachedTrack.dispose();
          }
          JniCommon.nativeReleaseRef(sender);
        });
  }

  /** Returns a pointer to webrtc::RtpSenderInterface. */
  long getNativeRtpSender() {
    return lifecycleLock.getNativePointer();
  }

  @NativeMethods
  interface Natives {
    boolean setTrack(long rtpSender, long nativeTrack);

    long getTrack(long rtpSender);

    void setStreams(long rtpSender, List<String> streamIds);

    List<String> getStreams(long rtpSender);

    long getDtmfSender(long rtpSender);

    boolean setParameters(long rtpSender, RtpParameters parameters);

    RtpParameters getParameters(long rtpSender);

    String getId(long rtpSender);

    void setFrameEncryptor(long rtpSender, long nativeFrameEncryptor);

    String getMediaType(long rtpSender);
  }
}
