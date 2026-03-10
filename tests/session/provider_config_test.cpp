#include <gtest/gtest.h>

#include "vibe/session/provider_config.h"

namespace vibe::session {
namespace {

TEST(ProviderConfigTest, ReturnsCodexDefaults) {
  const ProviderConfig config = DefaultProviderConfig(ProviderType::Codex);

  EXPECT_EQ(config.type, ProviderType::Codex);
  EXPECT_EQ(config.executable, "codex");
  EXPECT_TRUE(config.default_args.empty());
  EXPECT_TRUE(config.environment_overrides.empty());
}

TEST(ProviderConfigTest, ReturnsClaudeDefaults) {
  const ProviderConfig config = DefaultProviderConfig(ProviderType::Claude);

  EXPECT_EQ(config.type, ProviderType::Claude);
  EXPECT_EQ(config.executable, "claude");
  EXPECT_TRUE(config.default_args.empty());
  EXPECT_TRUE(config.environment_overrides.empty());
}

}  // namespace
}  // namespace vibe::session
