#ifndef VIBE_NET_HTTP_SHARED_H
#define VIBE_NET_HTTP_SHARED_H

#include <boost/beast/http.hpp>

#include "vibe/service/session_manager.h"

namespace vibe::net {

namespace http = boost::beast::http;

using HttpRequest = http::request<http::string_body>;
using HttpResponse = http::response<http::string_body>;

[[nodiscard]] auto HandleRequest(const HttpRequest& request,
                                 vibe::service::SessionManager& session_manager) -> HttpResponse;

}  // namespace vibe::net

#endif
