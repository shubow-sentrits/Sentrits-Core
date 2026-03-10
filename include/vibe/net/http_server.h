#ifndef VIBE_NET_HTTP_SERVER_H
#define VIBE_NET_HTTP_SERVER_H

#include <cstdint>
#include <string>

#include "vibe/service/session_manager.h"

namespace vibe::net {

class HttpServer {
 public:
  HttpServer(std::string bind_address, std::uint16_t port);

  [[nodiscard]] auto Run() -> bool;

 private:
  std::string bind_address_;
  std::uint16_t port_;
  vibe::service::SessionManager session_manager_;
};

}  // namespace vibe::net

#endif
