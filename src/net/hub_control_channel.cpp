#include "vibe/net/hub_control_channel.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <random>
#include <string>

namespace vibe::net {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

// ---------------------------------------------------------------------------
// RelayTokenStore
// ---------------------------------------------------------------------------

namespace {

auto GenerateRelayToken() -> std::string {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<unsigned> dist(0, 255);
  std::string token = "relay_";
  token.reserve(6 + 64);
  constexpr char hex[] = "0123456789abcdef";
  for (int i = 0; i < 32; ++i) {
    const unsigned byte = dist(rng);
    token += hex[byte >> 4];
    token += hex[byte & 0xf];
  }
  return token;
}

}  // namespace

auto RelayTokenStore::Issue(const std::string& session_id) -> std::string {
  const std::string token = GenerateRelayToken();
  const auto expires_at = std::chrono::steady_clock::now() + kTokenTTL;
  std::lock_guard lock(mutex_);
  tokens_[token] = Entry{session_id, expires_at};
  return token;
}

auto RelayTokenStore::ConsumeIfValid(const std::string& token,
                                     const std::string& requested_session_id) -> bool {
  std::lock_guard lock(mutex_);
  const auto it = tokens_.find(token);
  if (it == tokens_.end()) {
    return false;
  }
  const bool expired = std::chrono::steady_clock::now() > it->second.expires_at;
  const bool session_match = it->second.session_id == requested_session_id;
  tokens_.erase(it);  // consume regardless — prevent replay on any path
  return !expired && session_match;
}

void RelayTokenStore::PruneExpired() {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard lock(mutex_);
  for (auto it = tokens_.begin(); it != tokens_.end();) {
    if (now > it->second.expires_at) {
      it = tokens_.erase(it);
    } else {
      ++it;
    }
  }
}

// ---------------------------------------------------------------------------
// Internal helpers shared by control loop and bridge threads
// ---------------------------------------------------------------------------

namespace {

struct ParsedUrl {
  std::string scheme;
  std::string host;
  std::string port;
  std::string path;  // everything after authority
};

auto ParseUrl(const std::string& url) -> std::optional<ParsedUrl> {
  ParsedUrl result;
  std::string remainder;

  if (url.starts_with("wss://")) {
    result.scheme = "wss";
    result.port = "443";
    remainder = url.substr(6);
  } else if (url.starts_with("ws://")) {
    result.scheme = "ws";
    result.port = "80";
    remainder = url.substr(5);
  } else if (url.starts_with("https://")) {
    result.scheme = "wss";
    result.port = "443";
    remainder = url.substr(8);
  } else if (url.starts_with("http://")) {
    result.scheme = "ws";
    result.port = "80";
    remainder = url.substr(7);
  } else {
    return std::nullopt;
  }

  const auto slash_pos = remainder.find('/');
  const std::string authority =
      (slash_pos == std::string::npos) ? remainder : remainder.substr(0, slash_pos);
  result.path = (slash_pos == std::string::npos) ? "/" : remainder.substr(slash_pos);

  const auto colon_pos = authority.find(':');
  if (colon_pos != std::string::npos) {
    result.host = authority.substr(0, colon_pos);
    result.port = authority.substr(colon_pos + 1);
  } else {
    result.host = authority;
  }

  if (result.host.empty()) {
    return std::nullopt;
  }
  return result;
}

// Build TLS context for outbound Hub connections.
auto MakeHubSslContext(const HubControlChannelOptions& options) -> asio::ssl::context {
  asio::ssl::context ctx{asio::ssl::context::tls_client};
  if (options.use_default_verify_paths) {
    ctx.set_default_verify_paths();
  }
  if (options.ca_certificate_file.has_value()) {
    ctx.load_verify_file(*options.ca_certificate_file);
  }
  ctx.set_verify_mode(asio::ssl::verify_peer);
  return ctx;
}

// Connect + TLS handshake a Beast WSS stream. Returns false on error.
template <typename WsStream>
auto ConnectWss(WsStream& ws, const ParsedUrl& url, const std::string& bearer_token,
                const std::string& ws_path,
                const std::chrono::seconds connect_timeout) -> bool {
  boost::system::error_code ec;
  asio::io_context ioc;
  tcp::resolver resolver(ioc);

  const auto results = resolver.resolve(url.host, url.port, ec);
  if (ec) {
    std::cerr << "[hub-ctrl] resolve " << url.host << ':' << url.port
              << " failed: " << ec.message() << '\n';
    return false;
  }

  beast::get_lowest_layer(ws).expires_after(connect_timeout);
  beast::get_lowest_layer(ws).connect(results, ec);
  if (ec) {
    std::cerr << "[hub-ctrl] connect failed: " << ec.message() << '\n';
    return false;
  }

  // TLS handshake.
  if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), url.host.c_str())) {
    std::cerr << "[hub-ctrl] SNI failed\n";
    return false;
  }
  ws.next_layer().next_layer().expires_after(connect_timeout);
  ws.next_layer().handshake(asio::ssl::stream_base::client, ec);
  if (ec) {
    std::cerr << "[hub-ctrl] TLS handshake failed: " << ec.message() << '\n';
    return false;
  }

  // WebSocket handshake.
  beast::get_lowest_layer(ws).expires_never();
  ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
  ws.set_option(websocket::stream_base::decorator([&bearer_token](websocket::request_type& req) {
    req.set(http::field::authorization, "Bearer " + bearer_token);
  }));
  ws.handshake(url.host, ws_path, ec);
  if (ec) {
    std::cerr << "[hub-ctrl] WS handshake failed: " << ec.message() << '\n';
    return false;
  }
  return true;
}

// Connect a plain (non-TLS) Beast WS stream.
template <typename WsStream>
auto ConnectWs(WsStream& ws, const ParsedUrl& url, const std::string& bearer_token,
               const std::string& ws_path,
               const std::chrono::seconds connect_timeout) -> bool {
  boost::system::error_code ec;
  asio::io_context ioc;
  tcp::resolver resolver(ioc);

  const auto results = resolver.resolve(url.host, url.port, ec);
  if (ec) {
    std::cerr << "[hub-ctrl] resolve local " << url.host << ':' << url.port
              << " failed: " << ec.message() << '\n';
    return false;
  }

  beast::get_lowest_layer(ws).expires_after(connect_timeout);
  beast::get_lowest_layer(ws).connect(results, ec);
  if (ec) {
    std::cerr << "[hub-ctrl] local connect failed: " << ec.message() << '\n';
    return false;
  }

  beast::get_lowest_layer(ws).expires_never();
  ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
  ws.set_option(websocket::stream_base::decorator([&bearer_token](websocket::request_type& req) {
    req.set(http::field::authorization, "Bearer " + bearer_token);
  }));
  ws.handshake(url.host + ':' + url.port, ws_path, ec);
  if (ec) {
    std::cerr << "[hub-ctrl] local WS handshake failed: " << ec.message() << '\n';
    return false;
  }
  return true;
}

// Pipe bytes between two open WebSocket streams using async I/O on a shared
// io_context. Runs until either side closes, an error occurs, or should_stop()
// returns true (polled every 100 ms via a steady_timer). Caller must call
// ioc.run() after returning from this function — or this function runs ioc
// internally (see below).
//
// This function runs ioc.run() internally and returns when piping is complete.
// Both streams must be constructed with ioc.
template <typename StreamA, typename StreamB>
void PipeWebSocketsAsync(asio::io_context& ioc, StreamA& a, StreamB& b,
                         std::function<bool()> should_stop) {
  beast::flat_buffer buf_a, buf_b;
  asio::steady_timer stop_timer(ioc);

  // Declared before lambdas so they can reference each other recursively.
  std::function<void()> read_from_a;
  std::function<void()> read_from_b;
  std::function<void()> arm_stop_timer;

  read_from_a = [&]() {
    buf_a.clear();
    a.async_read(buf_a, [&](boost::system::error_code ec, std::size_t) {
      if (ec) {
        stop_timer.cancel();
        beast::get_lowest_layer(b).cancel();
        return;
      }
      b.text(a.got_text());
      boost::system::error_code wec;
      b.write(buf_a.data(), wec);
      if (wec) {
        stop_timer.cancel();
        beast::get_lowest_layer(a).cancel();
        return;
      }
      read_from_a();
    });
  };

  read_from_b = [&]() {
    buf_b.clear();
    b.async_read(buf_b, [&](boost::system::error_code ec, std::size_t) {
      if (ec) {
        stop_timer.cancel();
        beast::get_lowest_layer(a).cancel();
        return;
      }
      a.text(b.got_text());
      boost::system::error_code wec;
      a.write(buf_b.data(), wec);
      if (wec) {
        stop_timer.cancel();
        beast::get_lowest_layer(b).cancel();
        return;
      }
      read_from_b();
    });
  };

  arm_stop_timer = [&]() {
    stop_timer.expires_after(std::chrono::milliseconds(100));
    stop_timer.async_wait([&](boost::system::error_code ec) {
      if (ec) return;  // cancelled — either side closed or we already stopped
      if (should_stop && should_stop()) {
        beast::get_lowest_layer(a).cancel();
        beast::get_lowest_layer(b).cancel();
        return;
      }
      arm_stop_timer();
    });
  };

  if (should_stop) arm_stop_timer();
  read_from_a();
  read_from_b();
  ioc.run();
}

}  // namespace

// ---------------------------------------------------------------------------
// HubControlChannel
// ---------------------------------------------------------------------------

HubControlChannel::HubControlChannel(std::string hub_url, std::string hub_token,
                                     std::uint16_t local_port, bool local_tls,
                                     IssueRelayTokenFn issue_relay_token,
                                     HubControlChannelOptions options)
    : hub_url_(std::move(hub_url)),
      hub_token_(std::move(hub_token)),
      local_port_(local_port),
      local_tls_(local_tls),
      issue_relay_token_(std::move(issue_relay_token)),
      options_(std::move(options)) {}

HubControlChannel::~HubControlChannel() {
  Stop();
}

void HubControlChannel::Start() {
  std::lock_guard lock(mutex_);
  if (control_thread_.joinable()) {
    return;
  }
  stop_ = false;
  control_thread_ = std::thread(&HubControlChannel::RunControlLoop, this);
}

void HubControlChannel::Stop() {
  std::vector<std::shared_ptr<asio::io_context>> bridge_iocs;
  {
    std::lock_guard lock(mutex_);
    stop_ = true;
    if (current_ioc_) current_ioc_->stop();
  }
  {
    std::lock_guard lock(bridges_mutex_);
    bridge_iocs = bridge_iocs_;
  }
  for (const auto& ioc : bridge_iocs) {
    if (ioc) {
      ioc->stop();
    }
  }
  cv_.notify_all();
  if (control_thread_.joinable()) {
    control_thread_.join();
  }
  // Join all bridge threads.
  std::vector<std::thread> bridges;
  {
    std::lock_guard lock(bridges_mutex_);
    bridges = std::move(bridge_threads_);
  }
  for (auto& t : bridges) {
    if (t.joinable()) {
      t.join();
    }
  }
}

void HubControlChannel::ReapFinishedBridges() {
  std::lock_guard lock(bridges_mutex_);
  bridge_threads_.erase(
      std::remove_if(bridge_threads_.begin(), bridge_threads_.end(),
                     [](std::thread& t) {
                       if (!t.joinable()) return true;
                       // Can't check if done without join; leave for Stop().
                       return false;
                     }),
      bridge_threads_.end());
}

// Dispatch a relay.requested message through the async read handler.
// Called from within the io_context event loop (from async_read callback).
static void HandleControlMessage(const beast::flat_buffer& buf,
                                 std::function<void(const std::string&,
                                                     const std::string&)> on_relay_requested) {
  const std::string msg_str(static_cast<const char*>(buf.data().data()), buf.size());
  try {
    const auto obj = json::parse(msg_str).as_object();
    const std::string type(obj.at("type").as_string());
    if (type == "relay.requested") {
      const std::string channel_id(obj.at("channel_id").as_string());
      const std::string session_id(obj.at("session_id").as_string());
      on_relay_requested(channel_id, session_id);
    }
  } catch (const std::exception& e) {
    std::cerr << "[hub-ctrl] malformed control message: " << e.what() << '\n';
  }
}

void HubControlChannel::RunControlLoop() {
  const auto parsed = ParseUrl(hub_url_);
  if (!parsed.has_value()) {
    std::cerr << "[hub-ctrl] invalid hub_url: " << hub_url_ << '\n';
    return;
  }

  while (true) {
    {
      std::unique_lock lock(mutex_);
      if (stop_) return;
    }

    // Helper: send initial session inventory if configured.
    auto maybe_send_inventory = [&](auto& ws) {
      if (!options_.list_sessions_fn) return;
      const std::string sessions_json = options_.list_sessions_fn();
      json::object inv_msg;
      inv_msg["type"] = "session.inventory";
      inv_msg["sessions"] = json::parse(sessions_json);
      boost::system::error_code ec;
      ws.text(true);
      ws.write(asio::buffer(json::serialize(inv_msg)), ec);
      if (ec) {
        std::cerr << "[hub-ctrl] failed to send session inventory: " << ec.message() << '\n';
      }
    };

    // Helper: run the async read loop on an already-connected websocket stream.
    // Uses ioc.run(); the caller must have stored current_ioc_ = &ioc beforehand.
    auto run_read_loop = [&](auto& ws, auto& ioc) {
      beast::flat_buffer buf;
      std::function<void()> post_read;
      post_read = [&]() {
        buf.clear();
        ws.async_read(buf, [&](boost::system::error_code ec, std::size_t) {
          if (ec) {
            if (ec != asio::error::operation_aborted) {
              std::cerr << "[hub-ctrl] control channel read error: " << ec.message() << '\n';
            }
            return;
          }
          HandleControlMessage(buf,
              [this](const std::string& ch, const std::string& sess) {
                HandleRelayRequested(ch, sess);
              });
          post_read();
        });
      };
      post_read();
      ioc.run();
    };

    bool connected = false;
    if (parsed->scheme == "wss") {
      asio::io_context ioc;
      auto ssl_ctx = MakeHubSslContext(options_);
      ssl_ctx.set_verify_callback(asio::ssl::host_name_verification(parsed->host));

      websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws(ioc, ssl_ctx);

      {
        std::unique_lock lock(mutex_);
        if (stop_) return;
        current_ioc_ = &ioc;
      }
      if (ConnectWss(ws, *parsed, hub_token_, "/api/v1/hosts/stream",
                     options_.connect_timeout)) {
        connected = true;
        std::cout << "[hub-ctrl] control channel connected\n";
        maybe_send_inventory(ws);
        run_read_loop(ws, ioc);
      }
      {
        std::unique_lock lock(mutex_);
        current_ioc_ = nullptr;
      }
    } else {
      // Plain WS (e.g. local dev Hub without TLS).
      asio::io_context ioc;
      websocket::stream<beast::tcp_stream> ws(ioc);

      {
        std::unique_lock lock(mutex_);
        if (stop_) return;
        current_ioc_ = &ioc;
      }
      if (ConnectWs(ws, *parsed, hub_token_, "/api/v1/hosts/stream",
                    options_.connect_timeout)) {
        connected = true;
        std::cout << "[hub-ctrl] control channel connected (plain WS)\n";
        maybe_send_inventory(ws);
        run_read_loop(ws, ioc);
      }
      {
        std::unique_lock lock(mutex_);
        current_ioc_ = nullptr;
      }
    }

    if (!connected) {
      std::cerr << "[hub-ctrl] could not connect to Hub control channel\n";
    }

    // Wait before reconnecting, unless Stop() was called.
    std::unique_lock lock(mutex_);
    cv_.wait_for(lock, options_.reconnect_delay, [this] { return stop_; });
    if (stop_) return;
  }
}

void HubControlChannel::HandleRelayRequested(const std::string& channel_id,
                                             const std::string& session_id) {
  if (channel_id.empty() || session_id.empty()) {
    return;
  }
  {
    std::lock_guard lock(mutex_);
    if (stop_) {
      return;
    }
  }

  const std::string relay_token = issue_relay_token_(session_id);
  if (relay_token.empty()) {
    std::cerr << "[hub-ctrl] failed to issue relay token for session " << session_id << '\n';
    return;
  }

  // Spawn bridge thread. Tracked for joining on Stop().
  std::lock_guard lock(bridges_mutex_);
  bridge_threads_.emplace_back(&HubControlChannel::RunRelayBridge, this,
                               channel_id, session_id, relay_token);
}

void HubControlChannel::RunRelayBridge(std::string channel_id, std::string session_id,
                                       std::string relay_token) {
  const auto hub_parsed = ParseUrl(hub_url_);
  if (!hub_parsed.has_value()) {
    return;
  }

  const std::string local_path =
      "/ws/sessions/" + session_id + "?access_token=" + relay_token;
  const std::string hub_relay_path = "/api/v1/relay/host/" + channel_id;

  const ParsedUrl local_url{
      .scheme = local_tls_ ? "wss" : "ws",
      .host = "127.0.0.1",
      .port = std::to_string(local_port_),
      .path = local_path,
  };

  auto stop_pred = [this]() -> bool {
    std::lock_guard lock(mutex_);
    return stop_;
  };

  auto register_bridge_ioc = [&](const std::shared_ptr<asio::io_context>& ioc) {
    std::lock_guard lock(bridges_mutex_);
    bridge_iocs_.push_back(ioc);
  };
  auto unregister_bridge_ioc = [&](const std::shared_ptr<asio::io_context>& ioc) {
    std::lock_guard lock(bridges_mutex_);
    bridge_iocs_.erase(std::remove(bridge_iocs_.begin(), bridge_iocs_.end(), ioc),
                       bridge_iocs_.end());
  };

  // Connect local side and pipe, given an already-connected hub_ws.
  // Both hub_ws and local_ws share ioc so PipeWebSocketsAsync can run them together.
  auto pipe_with_local = [&](auto& hub_ws, const std::shared_ptr<asio::io_context>& ioc) {
    bool piped = false;
    if (local_tls_) {
      asio::ssl::context local_ssl{asio::ssl::context::tls_client};
      local_ssl.set_verify_mode(asio::ssl::verify_none);  // loopback — skip cert check
      websocket::stream<beast::ssl_stream<beast::tcp_stream>> local_ws(*ioc, local_ssl);
      if (ConnectWss(local_ws, local_url, relay_token, local_path,
                     options_.connect_timeout)) {
        std::cout << "[hub-ctrl] relay bridge active: channel=" << channel_id
                  << " session=" << session_id << '\n';
        PipeWebSocketsAsync(*ioc, hub_ws, local_ws, stop_pred);
        piped = true;
      } else {
        std::cerr << "[hub-ctrl] relay bridge: failed to connect local (TLS) side for session "
                  << session_id << '\n';
      }
    } else {
      websocket::stream<beast::tcp_stream> local_ws(*ioc);
      if (ConnectWs(local_ws, local_url, relay_token, local_path,
                    options_.connect_timeout)) {
        std::cout << "[hub-ctrl] relay bridge active: channel=" << channel_id
                  << " session=" << session_id << '\n';
        PipeWebSocketsAsync(*ioc, hub_ws, local_ws, stop_pred);
        piped = true;
      } else {
        std::cerr << "[hub-ctrl] relay bridge: failed to connect local side for session "
                  << session_id << '\n';
      }
    }
    if (!piped) {
      boost::system::error_code ignore;
      hub_ws.close(websocket::close_code::going_away, ignore);
    }
  };

  if (hub_parsed->scheme == "wss") {
    auto ioc = std::make_shared<asio::io_context>();
    register_bridge_ioc(ioc);
    auto ssl_ctx = MakeHubSslContext(options_);
    ssl_ctx.set_verify_callback(asio::ssl::host_name_verification(hub_parsed->host));
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> hub_ws(*ioc, ssl_ctx);
    if (!ConnectWss(hub_ws, *hub_parsed, hub_token_, hub_relay_path,
                    options_.connect_timeout)) {
      unregister_bridge_ioc(ioc);
      std::cerr << "[hub-ctrl] relay bridge: failed to connect Hub side for channel "
                << channel_id << '\n';
      return;
    }
    pipe_with_local(hub_ws, ioc);
    unregister_bridge_ioc(ioc);
  } else {
    auto ioc = std::make_shared<asio::io_context>();
    register_bridge_ioc(ioc);
    websocket::stream<beast::tcp_stream> hub_ws(*ioc);
    if (!ConnectWs(hub_ws, *hub_parsed, hub_token_, hub_relay_path,
                   options_.connect_timeout)) {
      unregister_bridge_ioc(ioc);
      std::cerr << "[hub-ctrl] relay bridge: failed to connect Hub side (plain) for channel "
                << channel_id << '\n';
      return;
    }
    pipe_with_local(hub_ws, ioc);
    unregister_bridge_ioc(ioc);
  }

  std::cout << "[hub-ctrl] relay bridge closed: channel=" << channel_id << '\n';
}

}  // namespace vibe::net
