#ifndef VIBE_NET_HUB_CLIENT_H
#define VIBE_NET_HUB_CLIENT_H

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <optional>

#include <boost/asio/io_context.hpp>

#include "vibe/service/session_manager.h"

namespace vibe::net {

// HubClient runs an outbound background thread that sends periodic heartbeats
// to Sentrits-Hub. It is optional: if hub_url or hub_token are absent the
// server operates in local-only mode. All Hub failures are non-fatal and logged
// to stderr; the server continues normally if Hub is unreachable.
struct HubClientOptions {
  std::chrono::milliseconds heartbeat_interval{std::chrono::seconds(30)};
  std::chrono::milliseconds request_timeout{std::chrono::seconds(5)};
  bool use_default_verify_paths{true};
  std::optional<std::string> ca_certificate_file;
};

class HubClient {
 public:
  HubClient(std::string hub_url, std::string hub_token,
            vibe::service::SessionManager& session_manager,
            HubClientOptions options = {});
  ~HubClient();

  // Non-copyable, non-movable.
  HubClient(const HubClient&) = delete;
  auto operator=(const HubClient&) = delete;

  // io_context must outlive the HubClient thread. Pass the server's io_context
  // so session snapshots are collected on the owning thread.
  void Start(boost::asio::io_context& io_context);
  void Stop();

 private:
  void RunLoop();
  void SendHeartbeat();

  std::string hub_url_;
  std::string hub_token_;
  vibe::service::SessionManager& session_manager_;
  HubClientOptions options_;
  boost::asio::io_context* io_context_{nullptr};

  std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_{false};
  std::thread thread_;
};

}  // namespace vibe::net

#endif
