#include <gtest/gtest.h>

#include "vibe/cli/daemon_client.h"

namespace vibe::cli {
namespace {

TEST(DaemonClientTest, BuildsCreateSessionRequestBody) {
  const std::string body =
      BuildCreateSessionRequestBody(vibe::session::ProviderType::Codex, "/tmp/project", "demo");

  EXPECT_NE(body.find("\"provider\":\"codex\""), std::string::npos);
  EXPECT_NE(body.find("\"workspaceRoot\":\"/tmp/project\""), std::string::npos);
  EXPECT_NE(body.find("\"title\":\"demo\""), std::string::npos);
}

TEST(DaemonClientTest, ParsesCreatedSessionId) {
  const auto session_id = ParseCreatedSessionId(R"({"sessionId":"s_42","status":"Running"})");
  ASSERT_TRUE(session_id.has_value());
  EXPECT_EQ(*session_id, "s_42");
}

TEST(DaemonClientTest, BuildsControlAndTerminalCommands) {
  const std::string control = BuildControlRequestCommand(vibe::session::ControllerKind::Host);
  EXPECT_NE(control.find("\"type\":\"session.control.request\""), std::string::npos);
  EXPECT_NE(control.find("\"kind\":\"host\""), std::string::npos);

  const std::string release = BuildReleaseControlCommand();
  EXPECT_NE(release.find("\"type\":\"session.control.release\""), std::string::npos);

  const std::string input = BuildInputCommand("hello\n");
  EXPECT_NE(input.find("\"type\":\"terminal.input\""), std::string::npos);
  EXPECT_NE(input.find("\"data\":\"hello\\n\""), std::string::npos);

  const std::string resize = BuildResizeCommand(
      vibe::session::TerminalSize{.columns = 90, .rows = 30});
  EXPECT_NE(resize.find("\"type\":\"terminal.resize\""), std::string::npos);
  EXPECT_NE(resize.find("\"cols\":90"), std::string::npos);
  EXPECT_NE(resize.find("\"rows\":30"), std::string::npos);
}

TEST(DaemonClientTest, RejectsMalformedCreateSessionResponse) {
  EXPECT_FALSE(ParseCreatedSessionId("{}").has_value());
  EXPECT_FALSE(ParseCreatedSessionId("not-json").has_value());
}

}  // namespace
}  // namespace vibe::cli
