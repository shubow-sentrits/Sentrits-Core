#ifndef VIBE_STORE_HOST_CONFIG_STORE_H
#define VIBE_STORE_HOST_CONFIG_STORE_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Default maximum number of recent launch records to retain per host.
inline constexpr std::size_t kDefaultMaxLaunchRecords = 50;

#include "vibe/session/session_types.h"

namespace vibe::store {

inline constexpr std::string_view kDefaultAdminHost = "127.0.0.1";
inline constexpr std::uint16_t kDefaultAdminPort = 18085;
inline constexpr std::string_view kDefaultRemoteHost = "0.0.0.0";
inline constexpr std::uint16_t kDefaultRemotePort = 18086;
inline constexpr std::string_view kDefaultDisplayName = "Sentrits Host";

struct ProviderCommandOverride {
  std::string executable;
  std::vector<std::string> args;

  [[nodiscard]] auto operator==(const ProviderCommandOverride& other) const -> bool = default;
};

// A bounded recent-launch record. Records are auto-saved when sessions are
// created and trimmed to max_launch_records. There is no named-preset model;
// client-side pin/favorite is the appropriate curation layer.
struct LaunchRecord {
  std::string record_id;
  vibe::session::ProviderType provider{vibe::session::ProviderType::Codex};
  std::string workspace_root;
  std::string title;
  std::int64_t launched_at_unix_ms{0};
  std::optional<std::string> conversation_id;
  std::vector<std::string> group_tags;
  std::optional<std::vector<std::string>> command_argv;
  std::optional<std::string> command_shell;

  [[nodiscard]] auto operator==(const LaunchRecord& other) const -> bool = default;
};

struct HostIdentity {
  std::string host_id;
  std::string display_name;
  std::string certificate_pem_path;
  std::string private_key_pem_path;
  std::string admin_host{std::string(kDefaultAdminHost)};
  std::uint16_t admin_port{kDefaultAdminPort};
  std::string remote_host{std::string(kDefaultRemoteHost)};
  std::uint16_t remote_port{kDefaultRemotePort};
  ProviderCommandOverride codex_command;
  ProviderCommandOverride claude_command;
  std::vector<LaunchRecord> launch_records;
  std::size_t max_launch_records{kDefaultMaxLaunchRecords};

  // Daemon-wide environment policy (for session bootstrap).
  std::optional<std::string> bootstrap_shell_path;
  bool import_service_manager_environment{false};
  std::vector<std::string> service_manager_environment_allowlist;

  [[nodiscard]] auto operator==(const HostIdentity& other) const -> bool = default;
};

[[nodiscard]] inline auto MakeDefaultHostIdentity() -> HostIdentity {
  return HostIdentity{
      .host_id = "",
      .display_name = std::string(kDefaultDisplayName),
      .certificate_pem_path = "",
      .private_key_pem_path = "",
      .admin_host = std::string(kDefaultAdminHost),
      .admin_port = kDefaultAdminPort,
      .remote_host = std::string(kDefaultRemoteHost),
      .remote_port = kDefaultRemotePort,
      .codex_command = {},
      .claude_command = {},
      .launch_records = {},
      .max_launch_records = kDefaultMaxLaunchRecords,
  };
}

class HostConfigStore {
 public:
  virtual ~HostConfigStore() = default;

  [[nodiscard]] virtual auto LoadHostIdentity() const -> std::optional<HostIdentity> = 0;
  [[nodiscard]] virtual auto SaveHostIdentity(const HostIdentity& identity) -> bool = 0;
};

[[nodiscard]] auto EnsureHostIdentity(HostConfigStore& store) -> std::optional<HostIdentity>;

}  // namespace vibe::store

#endif
