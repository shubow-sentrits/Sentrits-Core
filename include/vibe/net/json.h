#ifndef VIBE_NET_JSON_H
#define VIBE_NET_JSON_H

#include <string>
#include <string_view>
#include <vector>

#include "vibe/service/session_manager.h"
#include "vibe/session/session_snapshot.h"
#include "vibe/session/session_types.h"

namespace vibe::net {

[[nodiscard]] auto JsonEscape(std::string_view input) -> std::string;
[[nodiscard]] auto ToJson(const vibe::service::SessionSummary& summary) -> std::string;
[[nodiscard]] auto ToJson(const std::vector<vibe::service::SessionSummary>& summaries) -> std::string;
[[nodiscard]] auto ToJson(const vibe::session::SessionSnapshot& snapshot) -> std::string;
[[nodiscard]] auto ToJson(const vibe::session::OutputSlice& slice) -> std::string;
[[nodiscard]] auto ToJsonHostInfo() -> std::string;

}  // namespace vibe::net

#endif
