#ifndef VIBE_SESSION_PROVIDER_CONFIG_H
#define VIBE_SESSION_PROVIDER_CONFIG_H

#include <string>
#include <unordered_map>
#include <vector>

#include "vibe/session/session_types.h"

namespace vibe::session {

struct ProviderConfig {
  ProviderType type;
  std::string executable;
  std::vector<std::string> default_args;
  std::unordered_map<std::string, std::string> environment_overrides;
};

[[nodiscard]] auto DefaultProviderConfig(ProviderType provider) -> ProviderConfig;

}  // namespace vibe::session

#endif
