#include "vibe/session/provider_config.h"

namespace vibe::session {

auto DefaultProviderConfig(ProviderType provider) -> ProviderConfig {
  switch (provider) {
    case ProviderType::Codex:
      return ProviderConfig{
          .type = ProviderType::Codex,
          .executable = "codex",
          .default_args = {},
          .environment_overrides = {},
      };
    case ProviderType::Claude:
      return ProviderConfig{
          .type = ProviderType::Claude,
          .executable = "claude",
          .default_args = {},
          .environment_overrides = {},
      };
  }

  return ProviderConfig{
      .type = provider,
      .executable = {},
      .default_args = {},
      .environment_overrides = {},
  };
}

}  // namespace vibe::session
