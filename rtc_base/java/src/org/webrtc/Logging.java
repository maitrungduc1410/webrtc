/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.EnumSet;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;
import org.webrtc.Loggable;

/**
 * Java wrapper for WebRTC logging.
 *
 * Logging is configured once per process with Logging.initialize, which should be called before
 * PeerConnectionFactory.initialize. Once the native library is loaded, log messages from Java are
 * passed to native logging (webrtc::LogMessage), which writes them to logcat and to the sinks of
 * the LoggingConfig, together with the native log messages. This also applies when logging is
 * initialized in native code with webrtc::InitializeLogging. By default, release builds of WebRTC
 * don't write log messages to logcat, see LoggingConfig.Builder.setDebugSeverity.
 *
 * Before the native library is loaded, log messages from Java go to java.util.logging.Logger.
 */
public class Logging {
  private static final String TAG = "Logging";
  private static final Logger fallbackLogger = createFallbackLogger();
  private static final Severity[] severities = Severity.values();
  private static volatile boolean loggingEnabled;

  private static final Object initializationLock = new Object();

  // The config passed to initialize(), while it waits for the native library to be loaded.
  @GuardedBy("initializationLock")
  @Nullable
  private static LoggingConfig pendingConfig;

  @GuardedBy("initializationLock")
  private static boolean initializeCalled;

  @GuardedBy("initializationLock")
  private static boolean nativeLibraryLoaded;

  private static Logger createFallbackLogger() {
    final Logger fallbackLogger = Logger.getLogger("org.webrtc.Logging");
    fallbackLogger.setLevel(Level.ALL);
    return fallbackLogger;
  }

  // Keep in sync with webrtc/rtc_base/logging.h:LoggingSeverity.
  public enum Severity { LS_VERBOSE, LS_INFO, LS_WARNING, LS_ERROR, LS_NONE }

  /**
   * Initializes logging with `config`. Corresponds to webrtc::InitializeLogging.
   *
   * <p>Only the first call has an effect. Returns false if logging has already been initialized,
   * either by an earlier call or because native code has already logged.
   *
   * <p>Call this before PeerConnectionFactory.initialize, or before loading the native library in
   * an application that loads it itself. The config is then stored, which returns true, and
   * applied when the native library has been loaded, before other native code runs. If native
   * code has nevertheless logged by then, the config is ignored and a warning is logged.
   *
   * <p>If the native library is already loaded, the config is applied immediately.
   */
  public static boolean initialize(LoggingConfig config) {
    if (config == null) {
      throw new IllegalArgumentException("config may not be null.");
    }
    synchronized (initializationLock) {
      if (initializeCalled) {
        return false;
      }
      initializeCalled = true;
      if (!nativeLibraryLoaded) {
        pendingConfig = config;
        return true;
      }
      return applyConfig(config);
    }
  }

  /**
   * Applies the config that was passed to initialize() before the native library was loaded, and
   * passes log messages from Java to native logging from then on.
   *
   * <p>Called by native code when the library has been loaded, from JNI_OnLoad or InitAndroid(),
   * and by NativeLibrary. Only the first call has an effect.
   */
  @CalledByNative
  static void onNativeLibraryLoaded() {
    synchronized (initializationLock) {
      nativeLibraryLoaded = true;
      if (pendingConfig != null) {
        final LoggingConfig config = pendingConfig;
        pendingConfig = null;
        if (!applyConfig(config)) {
          // Logged before Java messages are passed to native logging, so that the warning reaches
          // logcat even if native logging doesn't write to it.
          w(TAG, "Native logging was already initialized. The LoggingConfig is ignored.");
        }
      }
      // Also when logging was initialized in native code with webrtc::InitializeLogging().
      loggingEnabled = true;
    }
  }

  /**
   * Lets initialize() be called again, as if the native library was not loaded. Doesn't reset
   * native logging.
   */
  @VisibleForTesting
  static void resetForTesting() {
    synchronized (initializationLock) {
      pendingConfig = null;
      initializeCalled = false;
      nativeLibraryLoaded = false;
      loggingEnabled = false;
    }
  }

  @SuppressWarnings("EnumOrdinal")
  private static boolean applyConfig(LoggingConfig config) {
    return LoggingJni.get().initialize(
        config.debugSeverity == null ? null : config.debugSeverity.ordinal(),
        config.minSeverity == null ? null : config.minSeverity.ordinal(),
        config.logThreads,
        config.logTimestamps,
        config.sinks);
  }

  // Passes a native log message to a sink that was added with LoggingConfig.Builder.addSink().
  @CalledByNative
  static void onNativeLogMessage(Loggable sink, String message, int severity, String tag) {
    sink.onLogMessage(message, severities[severity], tag);
  }

  @SuppressWarnings("EnumOrdinal")
  public static void log(Severity severity, String tag, String message) {
    if (tag == null || message == null) {
      throw new IllegalArgumentException("Logging tag or message may not be null.");
    }
    if (loggingEnabled) {
      LoggingJni.get().log(severity.ordinal(), tag, message);
      return;
    }

    // Fallback to system log.
    Level level;
    switch (severity) {
      case LS_ERROR:
        level = Level.SEVERE;
        break;
      case LS_WARNING:
        level = Level.WARNING;
        break;
      case LS_INFO:
        level = Level.INFO;
        break;
      default:
        level = Level.FINE;
        break;
    }
    fallbackLogger.log(level, tag + ": " + message);
  }

  public static void d(String tag, String message) {
    log(Severity.LS_INFO, tag, message);
  }

  public static void e(String tag, String message) {
    log(Severity.LS_ERROR, tag, message);
  }

  public static void w(String tag, String message) {
    log(Severity.LS_WARNING, tag, message);
  }

  public static void e(String tag, String message, Throwable e) {
    log(Severity.LS_ERROR, tag, message);
    log(Severity.LS_ERROR, tag, e.toString());
    log(Severity.LS_ERROR, tag, getStackTraceString(e));
  }

  public static void w(String tag, String message, Throwable e) {
    log(Severity.LS_WARNING, tag, message);
    log(Severity.LS_WARNING, tag, e.toString());
    log(Severity.LS_WARNING, tag, getStackTraceString(e));
  }

  public static void v(String tag, String message) {
    log(Severity.LS_VERBOSE, tag, message);
  }

  private static String getStackTraceString(Throwable e) {
    if (e == null) {
      return "";
    }

    StringWriter sw = new StringWriter();
    PrintWriter pw = new PrintWriter(sw);
    e.printStackTrace(pw);
    return sw.toString();
  }

  @NativeMethods
  interface Natives {
    // The severities are ordinals of Severity, or null to use the native default.
    boolean initialize(Integer debugSeverity, Integer minSeverity, boolean logThreads,
        boolean logTimestamps, Loggable[] sinks);

    void log(int severity, String tag, String message);
  }
}
