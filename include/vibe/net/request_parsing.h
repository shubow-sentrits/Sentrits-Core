#ifndef VIBE_NET_REQUEST_PARSING_H
#define VIBE_NET_REQUEST_PARSING_H

#include <cstddef>
#include <optional>
#include <string>

#include "vibe/service/session_manager.h"

namespace vibe::net {

[[nodiscard]] auto ParseCreateSessionRequest(const std::string& body)
    -> std::optional<vibe::service::CreateSessionRequest>;
[[nodiscard]] auto ParseInputRequest(const std::string& body) -> std::optional<std::string>;
[[nodiscard]] auto ParseTailBytes(const std::string& target) -> std::size_t;

}  // namespace vibe::net

#endif
