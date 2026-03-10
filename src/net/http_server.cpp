#include "vibe/net/http_server.h"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "vibe/net/http_shared.h"

namespace vibe::net {

namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;

namespace {

class HttpSession : public std::enable_shared_from_this<HttpSession> {
 public:
  HttpSession(tcp::socket socket, vibe::service::SessionManager& session_manager)
      : socket_(std::move(socket)),
        session_manager_(session_manager) {}

  void Start() { DoRead(); }

 private:
  void DoRead() {
    request_ = {};
    http::async_read(
        socket_, buffer_, request_,
        [self = shared_from_this()](const boost::system::error_code& error_code,
                                    const std::size_t /*bytes_transferred*/) {
          if (error_code) {
            return;
          }

          self->response_ = HandleRequest(self->request_, self->session_manager_);
          self->DoWrite();
        });
  }

  void DoWrite() {
    http::async_write(
        socket_, response_,
        [self = shared_from_this()](const boost::system::error_code& error_code,
                                    const std::size_t /*bytes_transferred*/) {
          boost::system::error_code shutdown_error;
          self->socket_.shutdown(tcp::socket::shutdown_send, shutdown_error);
          if (error_code) {
            return;
          }
        });
  }

  tcp::socket socket_;
  beast::flat_buffer buffer_;
  HttpRequest request_;
  HttpResponse response_;
  vibe::service::SessionManager& session_manager_;
};

class HttpListener : public std::enable_shared_from_this<HttpListener> {
 public:
  HttpListener(asio::io_context& io_context, const asio::ip::address& address,
               const std::uint16_t port, vibe::service::SessionManager& session_manager)
      : acceptor_(io_context),
        socket_(io_context),
        poll_timer_(io_context),
        session_manager_(session_manager) {
    boost::system::error_code error_code;
    const tcp::endpoint endpoint(address, port);

    acceptor_.open(endpoint.protocol(), error_code);
    if (error_code) {
      throw boost::system::system_error(error_code);
    }

    acceptor_.set_option(asio::socket_base::reuse_address(true), error_code);
    if (error_code) {
      throw boost::system::system_error(error_code);
    }

    acceptor_.bind(endpoint, error_code);
    if (error_code) {
      throw boost::system::system_error(error_code);
    }

    acceptor_.listen(asio::socket_base::max_listen_connections, error_code);
    if (error_code) {
      throw boost::system::system_error(error_code);
    }
  }

  void Start() {
    DoAccept();
    DoPoll();
  }

 private:
  void DoAccept() {
    acceptor_.async_accept(
        socket_, [self = shared_from_this()](const boost::system::error_code& error_code) {
          if (!error_code) {
            std::make_shared<HttpSession>(std::move(self->socket_), self->session_manager_)->Start();
          }

          self->DoAccept();
        });
  }

  void DoPoll() {
    poll_timer_.expires_after(std::chrono::milliseconds(50));
    poll_timer_.async_wait([self = shared_from_this()](const boost::system::error_code& error_code) {
      if (error_code == asio::error::operation_aborted) {
        return;
      }
      if (error_code) {
        std::cerr << "poll timer failed: " << error_code.message() << '\n';
        self->DoPoll();
        return;
      }

      self->session_manager_.PollAll(0);
      self->DoPoll();
    });
  }

  tcp::acceptor acceptor_;
  tcp::socket socket_;
  asio::steady_timer poll_timer_;
  vibe::service::SessionManager& session_manager_;
};

}  // namespace

HttpServer::HttpServer(std::string bind_address, const std::uint16_t port)
    : bind_address_(std::move(bind_address)),
      port_(port) {}

auto HttpServer::Run() -> bool {
  asio::io_context io_context{1};
  boost::system::error_code error_code;
  const auto address = asio::ip::make_address(bind_address_, error_code);
  if (error_code) {
    std::cerr << "invalid bind address " << bind_address_ << ": " << error_code.message() << '\n';
    return false;
  }

  try {
    std::make_shared<HttpListener>(io_context, address, port_, session_manager_)->Start();
  } catch (const boost::system::system_error& exception) {
    std::cerr << "failed to bind " << bind_address_ << ":" << port_ << ": "
              << exception.code().message() << '\n';
    return false;
  }

  std::cout << "HTTP server listening on " << bind_address_ << ":" << port_ << '\n';
  io_context.run();
  return true;
}

}  // namespace vibe::net
