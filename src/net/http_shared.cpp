#include "vibe/net/http_shared.h"

#include <string>

#include "vibe/net/json.h"
#include "vibe/net/request_parsing.h"

namespace vibe::net {

auto HandleRequest(const HttpRequest& request,
                   vibe::service::SessionManager& session_manager) -> HttpResponse {
  HttpResponse response;
  response.version(request.version());
  response.keep_alive(false);
  response.set(http::field::content_type, "application/json; charset=utf-8");

  if (request.method() == http::verb::get && request.target() == "/host/info") {
    response.result(http::status::ok);
    response.body() = ToJsonHostInfo();
    response.prepare_payload();
    return response;
  }

  if (request.method() == http::verb::get && request.target() == "/health") {
    response.result(http::status::ok);
    response.set(http::field::content_type, "text/plain; charset=utf-8");
    response.body() = "ok\n";
    response.prepare_payload();
    return response;
  }

  if (request.method() == http::verb::get && request.target() == "/sessions") {
    response.result(http::status::ok);
    response.body() = ToJson(session_manager.ListSessions());
    response.prepare_payload();
    return response;
  }

  if (request.method() == http::verb::post && request.target() == "/sessions") {
    const auto parsed_request = ParseCreateSessionRequest(request.body());
    if (!parsed_request.has_value()) {
      response.result(http::status::bad_request);
      response.body() = "{\"error\":\"invalid create session request\"}";
      response.prepare_payload();
      return response;
    }

    const auto created = session_manager.CreateSession(*parsed_request);

    if (!created.has_value()) {
      response.result(http::status::internal_server_error);
      response.body() = "{\"error\":\"failed to create session\"}";
      response.prepare_payload();
      return response;
    }

    response.result(http::status::created);
    response.body() = ToJson(*created);
    response.prepare_payload();
    return response;
  }

  const std::string target(request.target());
  const std::string sessions_prefix = "/sessions/";
  if (target.rfind(sessions_prefix, 0) == 0) {
    const std::string remainder = target.substr(sessions_prefix.size());
    const auto snapshot_suffix = std::string("/snapshot");
    const auto input_suffix = std::string("/input");
    const auto stop_suffix = std::string("/stop");
    const auto tail_marker = std::string("/tail");

    if (remainder.size() > snapshot_suffix.size() &&
        remainder.ends_with(snapshot_suffix)) {
      const std::string session_id =
          remainder.substr(0, remainder.size() - snapshot_suffix.size());
      const auto snapshot = session_manager.GetSnapshot(session_id);
      if (!snapshot.has_value()) {
        response.result(http::status::not_found);
        response.body() = "{\"error\":\"session not found\"}";
        response.prepare_payload();
        return response;
      }

      response.result(http::status::ok);
      response.body() = ToJson(*snapshot);
      response.prepare_payload();
      return response;
    }

    const std::size_t tail_pos = remainder.find(tail_marker);
    if (tail_pos != std::string::npos) {
      const std::string session_id = remainder.substr(0, tail_pos);
      const auto tail = session_manager.GetTail(session_id, ParseTailBytes(target));
      if (!tail.has_value()) {
        response.result(http::status::not_found);
        response.body() = "{\"error\":\"session not found\"}";
        response.prepare_payload();
        return response;
      }

      response.result(http::status::ok);
      response.body() = ToJson(*tail);
      response.prepare_payload();
      return response;
    }

    if (request.method() == http::verb::post && remainder.size() > input_suffix.size() &&
        remainder.ends_with(input_suffix)) {
      const std::string session_id =
          remainder.substr(0, remainder.size() - input_suffix.size());
      const auto input = ParseInputRequest(request.body());
      if (!input.has_value()) {
        response.result(http::status::bad_request);
        response.body() = "{\"error\":\"invalid input request\"}";
        response.prepare_payload();
        return response;
      }

      const bool wrote = session_manager.SendInput(session_id, *input);
      if (!wrote) {
        response.result(http::status::bad_request);
        response.body() = "{\"error\":\"unable to send input\"}";
        response.prepare_payload();
        return response;
      }

      response.result(http::status::ok);
      response.body() = "{\"status\":\"ok\"}";
      response.prepare_payload();
      return response;
    }

    if (request.method() == http::verb::post && remainder.size() > stop_suffix.size() &&
        remainder.ends_with(stop_suffix)) {
      const std::string session_id =
          remainder.substr(0, remainder.size() - stop_suffix.size());
      const bool stopped = session_manager.StopSession(session_id);
      if (!stopped) {
        response.result(http::status::bad_request);
        response.body() = "{\"error\":\"unable to stop session\"}";
        response.prepare_payload();
        return response;
      }

      response.result(http::status::ok);
      response.body() = "{\"status\":\"stopped\"}";
      response.prepare_payload();
      return response;
    }

    const auto summary = session_manager.GetSession(remainder);
    if (!summary.has_value()) {
      response.result(http::status::not_found);
      response.body() = "{\"error\":\"session not found\"}";
      response.prepare_payload();
      return response;
    }

    response.result(http::status::ok);
    response.body() = ToJson(*summary);
    response.prepare_payload();
    return response;
  }

  response.result(http::status::not_found);
  response.body() = "{\"error\":\"not found\"}";
  response.prepare_payload();
  return response;
}

}  // namespace vibe::net
