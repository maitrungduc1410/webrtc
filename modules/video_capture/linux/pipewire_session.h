/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_LINUX_PIPEWIRE_SESSION_H_
#define MODULES_VIDEO_CAPTURE_LINUX_PIPEWIRE_SESSION_H_

#include <pipewire/pipewire.h>
#include <spa/pod/pod.h>
#include <spa/utils/dict.h>
#include <spa/utils/hook.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "modules/portal/pipewire_utils.h"
#include "modules/portal/portal_guard.h"
#include "modules/portal/portal_request_response.h"
#include "modules/video_capture/linux/camera_portal.h"
#include "modules/video_capture/video_capture_defines.h"
#include "modules/video_capture/video_capture_options.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace videocapturemodule {

class PipeWireSession;
class VideoCaptureModulePipeWire;

// PipeWireNode objects are the local representation of PipeWire node objects.
// The portal API ensured that only camera nodes are visible to the client.
// So they all represent one camera that is available via PipeWire.
class PipeWireNode {
 public:
  struct PipeWireNodeDeleter {
    void operator()(PipeWireNode* node) const noexcept;
  };

  using PipeWireNodePtr =
      std::unique_ptr<PipeWireNode, PipeWireNode::PipeWireNodeDeleter>;
  static PipeWireNodePtr Create(PipeWireSession* session,
                                uint32_t id,
                                const std::string& object_serial,
                                const spa_dict* props);

  uint32_t id() const { return id_; }
  const std::string& object_serial() const { return object_serial_; }
  const std::string& display_name() const { return display_name_; }
  const std::string& unique_id() const { return unique_id_; }
  std::string model_id() const {
    webrtc::MutexLock lock(&lock_);
    return model_id_;
  }
  std::vector<VideoCaptureCapability> capabilities() const {
    webrtc::MutexLock lock(&lock_);
    return capabilities_;
  }

 protected:
  PipeWireNode(PipeWireSession* session,
               uint32_t id,
               const std::string& object_serial,
               const spa_dict* props);

 private:
  static void OnNodeInfo(void* data, const pw_node_info* info);
  static void OnNodeParam(void* data,
                          int seq,
                          uint32_t id,
                          uint32_t index,
                          uint32_t next,
                          const spa_pod* param);
  static bool ParseFormat(const spa_pod* param, VideoCaptureCapability* cap);
  static void OnProxyDone(void* data, int seq);

  struct pw_proxy* proxy_;
  struct spa_hook node_listener_;
  struct spa_hook proxy_listener_;
  PipeWireSession* session_;
  uint32_t id_;
  std::string object_serial_;
  std::string display_name_;
  std::string unique_id_;
  // The PipeWire thread changes these while other threads read them.
  mutable webrtc::Mutex lock_;
  std::string model_id_ RTC_GUARDED_BY(&lock_);
  struct pw_node_info* info_ = nullptr;
  int sync_seq_ = 0;
  std::vector<VideoCaptureCapability> capabilities_ RTC_GUARDED_BY(&lock_);
  std::vector<VideoCaptureCapability> pending_capabilities_;
};

class CameraPortalNotifier : public CameraPortal::PortalNotifier {
 public:
  CameraPortalNotifier(PipeWireSession* session);
  ~CameraPortalNotifier() override = default;

  void OnCameraRequestResult(xdg_portal::RequestResponse result,
                             int fd) override;

 private:
  PipeWireSession* session_;
};

class PipeWireSession : public webrtc::RefCountedNonVirtual<PipeWireSession> {
 public:
  PipeWireSession();
  ~PipeWireSession();

  void Init(VideoCaptureOptions::Callback* callback,
            int fd = kInvalidPipeWireFd);
  webrtc::Mutex& nodes_lock() RTC_LOCK_RETURNED(&nodes_lock_) {
    return nodes_lock_;
  }
  const std::deque<PipeWireNode::PipeWireNodePtr>& nodes() const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(&nodes_lock_) {
    return nodes_;
  }

  // True once the connection to PipeWire is lost and the session cannot
  // connect again, because its fd came from the application.
  bool connection_lost() const { return connection_lost_; }

  friend class CameraPortalNotifier;
  friend class PipeWireNode;
  friend class VideoCaptureModulePipeWire;

 private:
  void InitPipeWire(int fd);
  bool StartPipeWire(int fd);
  bool ConnectPipeWire(int fd);
  void StopPipeWire();
  void PipeWireSync();

  static void OnCoreError(void* data,
                          uint32_t id,
                          int seq,
                          int res,
                          const char* message);
  static void OnCoreDone(void* data, uint32_t id, int seq);

  // Called on the PipeWire thread when the connection to PipeWire is lost.
  void OnConnectionLost();
  // A reconnect asks the camera portal for a new fd, like Init(), on
  // portal_context_. ScheduleReconnect() can be called on any thread, the
  // others run on that context.
  void ScheduleReconnect(int delay_ms);
  static gboolean OnReconnect(gpointer user_data);
  void Reconnect();
  void OnReconnectResult(xdg_portal::RequestResponse result, int fd);

  static void OnRegistryGlobal(void* data,
                               uint32_t id,
                               uint32_t permissions,
                               const char* type,
                               uint32_t version,
                               const spa_dict* props);
  static void OnRegistryGlobalRemove(void* data, uint32_t id);

  void Finish(VideoCaptureOptions::Status status);
  void Cleanup();

  webrtc::Mutex callback_lock_;
  VideoCaptureOptions::Callback* callback_ RTC_GUARDED_BY(&callback_lock_) =
      nullptr;

  VideoCaptureOptions::Status status_;

  std::unique_ptr<PipeWireInitializer> pw_initializer_;
  struct pw_thread_loop* pw_main_loop_ = nullptr;
  struct pw_context* pw_context_ = nullptr;
  struct pw_core* pw_core_ = nullptr;
  struct spa_hook core_listener_;

  struct pw_registry* pw_registry_ = nullptr;
  struct spa_hook registry_listener_;

  int sync_seq_ = 0;

  // Only set when the session opened the camera portal itself: only the
  // portal can give it a new fd for a lost connection. The portal answers on
  // this context, the thread-default GLib main context of Init().
  GMainContext* portal_context_ = nullptr;
  // Tells reconnect attempts scheduled on portal_context_ that the session
  // is gone.
  scoped_refptr<PortalGuard> reconnect_guard_;
  // Used on portal_context_. Set when the session asks the portal again;
  // from then on, the portal results are for reconnects.
  bool reconnecting_ = false;
  // Delay before the next attempt after a failed one. The PipeWire thread
  // resets it when PipeWire answers.
  std::atomic<int> reconnect_delay_ms_ = 0;

  // Set on the PipeWire thread, read on any thread.
  std::atomic<bool> connection_lost_ = false;

  // The PipeWire thread adds and removes nodes, while DeviceInfoPipeWire and
  // VideoCaptureModulePipeWire read the list on other threads. Hold it while
  // using a node from the list: removing a node deletes it. Do not hold it
  // while calling out of the session, e.g. Finish(): the callback can list
  // the cameras.
  webrtc::Mutex nodes_lock_;
  std::deque<PipeWireNode::PipeWireNodePtr> nodes_ RTC_GUARDED_BY(&nodes_lock_);
  // Cameras found whose capabilities may not be known yet. They are not listed
  // until OnCoreDone(), as a camera cannot be opened without its capabilities.
  std::deque<PipeWireNode::PipeWireNodePtr> pending_nodes_
      RTC_GUARDED_BY(&nodes_lock_);
  // Counts the lost connections. When PipeWire comes back, it numbers its
  // nodes anew, so a node id is only valid while this count stays the same.
  int lost_connections_ RTC_GUARDED_BY(&nodes_lock_) = 0;
  std::unique_ptr<CameraPortal> portal_;
  std::unique_ptr<CameraPortalNotifier> portal_notifier_;
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_LINUX_PIPEWIRE_SESSION_H_
