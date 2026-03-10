#include <gtest/gtest.h>

#include "vibe/net/request_parsing.h"

namespace vibe::net {
namespace {

TEST(RequestParsingTest, ParsesCreateSessionRequestWithExplicitCommand) {
  const auto request = ParseCreateSessionRequest(
      R"({"provider":"claude","workspaceRoot":".","title":"demo","command":["/opt/homebrew/bin/claude","--print"]})");
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->provider, vibe::session::ProviderType::Claude);
  ASSERT_TRUE(request->command_argv.has_value());
  EXPECT_EQ(*request->command_argv,
            (std::vector<std::string>{"/opt/homebrew/bin/claude", "--print"}));
}

TEST(RequestParsingTest, RejectsInvalidExplicitCommandInCreateSessionRequest) {
  EXPECT_FALSE(ParseCreateSessionRequest(
                   R"({"provider":"codex","workspaceRoot":".","title":"demo","command":[]})")
                   .has_value());
  EXPECT_FALSE(ParseCreateSessionRequest(
                   R"({"provider":"codex","workspaceRoot":".","title":"demo","command":[""]})")
                   .has_value());
  EXPECT_FALSE(ParseCreateSessionRequest(
                   R"({"provider":"codex","workspaceRoot":".","title":"demo","command":"codex"})")
                   .has_value());
}

TEST(RequestParsingTest, ParsesWebSocketInputCommand) {
  const auto command = ParseWebSocketCommand(R"({"type":"terminal.input","data":"hello\n"})");
  ASSERT_TRUE(command.has_value());
  ASSERT_TRUE(std::holds_alternative<WebSocketInputCommand>(*command));
  EXPECT_EQ(std::get<WebSocketInputCommand>(*command).data, "hello\n");
}

TEST(RequestParsingTest, ParsesWebSocketResizeCommand) {
  const auto command = ParseWebSocketCommand(R"({"type":"terminal.resize","cols":80,"rows":24})");
  ASSERT_TRUE(command.has_value());
  ASSERT_TRUE(std::holds_alternative<WebSocketResizeCommand>(*command));
  EXPECT_EQ(std::get<WebSocketResizeCommand>(*command).terminal_size.columns, 80);
  EXPECT_EQ(std::get<WebSocketResizeCommand>(*command).terminal_size.rows, 24);
}

TEST(RequestParsingTest, ParsesWebSocketStopCommand) {
  const auto command = ParseWebSocketCommand(R"({"type":"session.stop"})");
  ASSERT_TRUE(command.has_value());
  EXPECT_TRUE(std::holds_alternative<WebSocketStopCommand>(*command));
}

TEST(RequestParsingTest, ParsesControlCommands) {
  const auto request_control = ParseWebSocketCommand(R"({"type":"session.control.request"})");
  ASSERT_TRUE(request_control.has_value());
  EXPECT_TRUE(std::holds_alternative<WebSocketRequestControlCommand>(*request_control));
  EXPECT_EQ(std::get<WebSocketRequestControlCommand>(*request_control).controller_kind,
            vibe::session::ControllerKind::Remote);

  const auto request_host_control =
      ParseWebSocketCommand(R"({"type":"session.control.request","kind":"host"})");
  ASSERT_TRUE(request_host_control.has_value());
  ASSERT_TRUE(std::holds_alternative<WebSocketRequestControlCommand>(*request_host_control));
  EXPECT_EQ(std::get<WebSocketRequestControlCommand>(*request_host_control).controller_kind,
            vibe::session::ControllerKind::Host);

  const auto release_control = ParseWebSocketCommand(R"({"type":"session.control.release"})");
  ASSERT_TRUE(release_control.has_value());
  EXPECT_TRUE(std::holds_alternative<WebSocketReleaseControlCommand>(*release_control));
}

TEST(RequestParsingTest, RejectsMalformedOrUnknownWebSocketCommands) {
  EXPECT_FALSE(ParseWebSocketCommand(R"({"type":"unknown"})").has_value());
  EXPECT_FALSE(ParseWebSocketCommand(R"({"type":"terminal.input"})").has_value());
  EXPECT_FALSE(ParseWebSocketCommand(R"({"type":"terminal.resize","cols":0,"rows":24})").has_value());
  EXPECT_FALSE(ParseWebSocketCommand("not-json").has_value());
}

}  // namespace
}  // namespace vibe::net
