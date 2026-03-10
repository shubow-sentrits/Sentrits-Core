#include "vibe/net/request_parsing.h"

#include <boost/json.hpp>

namespace vibe::net {

namespace json = boost::json;

auto ParseCreateSessionRequest(const std::string& body)
    -> std::optional<vibe::service::CreateSessionRequest> {
  boost::system::error_code error_code;
  const json::value parsed = json::parse(body, error_code);
  if (error_code || !parsed.is_object()) {
    return std::nullopt;
  }

  const json::object& object = parsed.as_object();
  const auto provider_value = object.if_contains("provider");
  const auto workspace_root = object.if_contains("workspaceRoot");
  const auto title = object.if_contains("title");

  if (provider_value == nullptr || workspace_root == nullptr || title == nullptr ||
      !provider_value->is_string() || !workspace_root->is_string() || !title->is_string()) {
    return std::nullopt;
  }

  const std::string provider_name = json::value_to<std::string>(*provider_value);
  vibe::session::ProviderType provider = vibe::session::ProviderType::Codex;
  if (provider_name == "claude") {
    provider = vibe::session::ProviderType::Claude;
  } else if (provider_name != "codex") {
    return std::nullopt;
  }

  return vibe::service::CreateSessionRequest{
      .provider = provider,
      .workspace_root = json::value_to<std::string>(*workspace_root),
      .title = json::value_to<std::string>(*title),
  };
}

auto ParseInputRequest(const std::string& body) -> std::optional<std::string> {
  boost::system::error_code error_code;
  const json::value parsed = json::parse(body, error_code);
  if (error_code || !parsed.is_object()) {
    return std::nullopt;
  }

  const json::object& object = parsed.as_object();
  const auto data = object.if_contains("data");
  if (data == nullptr || !data->is_string()) {
    return std::nullopt;
  }

  return json::value_to<std::string>(*data);
}

auto ParseTailBytes(const std::string& target) -> std::size_t {
  constexpr std::size_t default_tail_bytes = 65536;
  const std::string marker = "?bytes=";
  const std::size_t marker_pos = target.find(marker);
  if (marker_pos == std::string::npos) {
    return default_tail_bytes;
  }

  const std::size_t value_start = marker_pos + marker.size();
  const std::string value = target.substr(value_start);
  if (value.empty()) {
    return default_tail_bytes;
  }

  return static_cast<std::size_t>(std::stoul(value));
}

}  // namespace vibe::net
