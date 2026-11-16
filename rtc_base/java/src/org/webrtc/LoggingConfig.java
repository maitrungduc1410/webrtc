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
import java.util.ArrayList;
import java.util.List;
import org.webrtc.Logging.Severity;

/**
 * Configuration for {@link Logging#initialize}. Corresponds to webrtc::LoggingConfig.
 *
 * <p>Options that are not set use the defaults of webrtc::LoggingConfig.
 */
public final class LoggingConfig {
  @Nullable final Severity debugSeverity;
  @Nullable final Severity minSeverity;
  final boolean logThreads;
  final boolean logTimestamps;
  final Loggable[] sinks;

  private LoggingConfig(Builder builder) {
    this.debugSeverity = builder.debugSeverity;
    this.minSeverity = builder.minSeverity;
    this.logThreads = builder.logThreads;
    this.logTimestamps = builder.logTimestamps;
    this.sinks = builder.sinks.toArray(new Loggable[0]);
  }

  public static Builder builder() {
    return new Builder();
  }

  public static final class Builder {
    @Nullable private Severity debugSeverity;
    @Nullable private Severity minSeverity;
    private boolean logThreads;
    private boolean logTimestamps;
    private final List<Loggable> sinks = new ArrayList<>();

    private Builder() {}

    /**
     * Sets the minimum severity of the messages that are written to logcat. By default, debug
     * builds of WebRTC write messages from LS_INFO and release builds write none.
     */
    public Builder setDebugSeverity(Severity severity) {
      this.debugSeverity = severity;
      return this;
    }

    /** Sets the minimum severity of the messages that the sinks receive. Defaults to LS_INFO. */
    public Builder setMinSeverity(Severity severity) {
      this.minSeverity = severity;
      return this;
    }

    /** Sets whether log lines include the thread ID. Defaults to false. */
    public Builder setLogThreads(boolean logThreads) {
      this.logThreads = logThreads;
      return this;
    }

    /** Sets whether log lines include the elapsed time. Defaults to false. */
    public Builder setLogTimestamps(boolean logTimestamps) {
      this.logTimestamps = logTimestamps;
      return this;
    }

    /**
     * Adds a sink that receives the messages from Java and native code.
     *
     * <p>The sink receives log lines as formatted by native logging. A message that was logged
     * from Java with Logging arrives without the tag, which is passed separately. It comes after
     * the elapsed time and the thread ID if those are enabled, and ends with a newline.
     *
     * <p>The sink is kept for the lifetime of the process and can't be removed. It is called on the
     * thread that logs, without synchronization, so it must be thread-safe.
     */
    public Builder addSink(Loggable sink) {
      if (sink == null) {
        throw new IllegalArgumentException("sink may not be null.");
      }
      sinks.add(sink);
      return this;
    }

    public LoggingConfig build() {
      return new LoggingConfig(this);
    }
  }
}
