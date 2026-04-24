#include <gtest/gtest.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "vibe/net/hub_client.h"

namespace vibe::net {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace ssl = asio::ssl;
namespace http = beast::http;
using tcp = asio::ip::tcp;

constexpr std::string_view kTestCertificatePem =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDGjCCAgKgAwIBAgIUT2hpUIAsMRGW7REL2ziF7U6k6LswDQYJKoZIhvcNAQEL\n"
    "BQAwFDESMBAGA1UEAwwJMTI3LjAuMC4xMB4XDTI2MDQyNDE4MzEyOVoXDTI3MDQy\n"
    "NDE4MzEyOVowFDESMBAGA1UEAwwJMTI3LjAuMC4xMIIBIjANBgkqhkiG9w0BAQEF\n"
    "AAOCAQ8AMIIBCgKCAQEAtdOJK/BRmZDrCCqFa4ptIlQTPvc/n+LvMhbKHXjtSw0Y\n"
    "ZIZYZ+Mdx+zXFwAqfSr1r4Zg9tI69ZWy7cmm4k0+pcwspkeW9Rzaswugl+GsVk6K\n"
    "kYFdWUpoFFGZunxcW+MQS803KLNd+hS+gf0sgthtBVxwvP0FSFIQUbV5l1iGiKPs\n"
    "uXd0ZVHTyrgY8n8N+JZThs2cXTHBHDdZd7gHXCDZXeSoepIrCRHrmShx1kw08NRu\n"
    "NofPCUM0cqbt2O4C1MIejFmdhbCKh4niCaaJYyE720AwfPdaCqfA6xVQ6g7Nc/vr\n"
    "Zbd7dkarVob/g7bpz7p1zSQTzWd7pC87+AvgCWOdGQIDAQABo2QwYjAdBgNVHQ4E\n"
    "FgQUEhBbyg9zZqNGYvHPb+7HWCygm0EwHwYDVR0jBBgwFoAUEhBbyg9zZqNGYvHP\n"
    "b+7HWCygm0EwDwYDVR0TAQH/BAUwAwEB/zAPBgNVHREECDAGhwR/AAABMA0GCSqG\n"
    "SIb3DQEBCwUAA4IBAQAfXpOp2HqgYGHGFKI7b5pg1Nz+vnBur0KSoT8kbAX5kjOa\n"
    "JSAT0vqpnsbrkUPZBU0AOypNad+St835dpWrnH4dwf+fWdZsK8t+XzW6WHnK592+\n"
    "08/hT1mxrH8vU6qhaxygPAM6VZ33DM3fh01t3SR2atl+9aHq1qIVFkCsF/Vy7B4w\n"
    "g4DAeUBjylmtWcwF3gprKHO7E2Iw37i3Nn98AMeDO1AyE+iPGfFUGbo+9OfzDE3z\n"
    "RIIqBArKUzy71dZFI0yr9aVzUKFKj4yp6r1nFxNy3AwPmK79pMHgXStWsaHzO5pj\n"
    "df65WJYXlFO2fr5vIKhT6BiVF9hFTEmE+Cr3dfJn\n"
    "-----END CERTIFICATE-----\n";

constexpr std::string_view kTestPrivateKeyPem =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQC104kr8FGZkOsI\n"
    "KoVrim0iVBM+9z+f4u8yFsodeO1LDRhkhlhn4x3H7NcXACp9KvWvhmD20jr1lbLt\n"
    "yabiTT6lzCymR5b1HNqzC6CX4axWToqRgV1ZSmgUUZm6fFxb4xBLzTcos136FL6B\n"
    "/SyC2G0FXHC8/QVIUhBRtXmXWIaIo+y5d3RlUdPKuBjyfw34llOGzZxdMcEcN1l3\n"
    "uAdcINld5Kh6kisJEeuZKHHWTDTw1G42h88JQzRypu3Y7gLUwh6MWZ2FsIqHieIJ\n"
    "poljITvbQDB891oKp8DrFVDqDs1z++tlt3t2RqtWhv+DtunPunXNJBPNZ3ukLzv4\n"
    "C+AJY50ZAgMBAAECggEAA0BK4cYUchuqatp2c+5P4OnCRaweb0mbqk3IWQ+PpU8Q\n"
    "zRdZNFOrdXwxel/5N9oRS41VEC9rziFaR4XaszgwsxPmsSXIOA4chlANE+TSnovH\n"
    "+YMSfBATCANPguCfYXs6ut2VVhJ4TzTuFR1FnaSotQxVQw5+eNpoH/XlPIS2NHI7\n"
    "iFrPuI5Ta/TFu2vjrRL+1mbrdcWHBrLXeABXy//zNhvtyjH6gbD0yDEnsvOH8Jhc\n"
    "j0cmDLrXBOGFl7/yNgg1Cz7ibNK1grNpe7reo3r5QZBvciVGUUI5RHABTq1Jg75A\n"
    "4LolFnzO6/sdcWLY2HG7p15MgQDnGnQTuS+hve5ccwKBgQDYdacatd0We8NT093t\n"
    "omibT2198pfnX6qinfJMUXM2SrbKkPqZZw0x8UNZU27GwbmZOV7czBaT+7vP93Vk\n"
    "hSEXO8C7tlKlYb6sFvOZQ4AdH4NaoGk9pRCZZeucWtuKwF5zvLxmLARo1GDSFPOx\n"
    "3eLz7/XpibVtiPEgEKj++7x+bwKBgQDXClHhmN8ZOAxSmosKWlQK2nkpqWlJgLhe\n"
    "sxK1nVIqN3MReoTPWPOLoyziT0IE5QRelsPyFV2d8Lzq/idlVXj19R0yHzivD9Gm\n"
    "yeD9OkHRwQL+4M2nn0FUc5cQfeJ/UWUKpvkoTb2QgjbCwCYYKvPGzX915Sieplzo\n"
    "6JVACKRg9wKBgQCFNgKcwYdKKuhOUniloelWi08Kz50EWy+b3DAdH5MTum87wnU2\n"
    "quDH935HHr1xvA8IaPIkV8UdVTKEDfpE3lk6/x7hZpb+CGVbatSHYa8aPSaNQ2MA\n"
    "+PB6NusE0jWB8lkuSNx41GXyTaE4KITA1ZiyHt7r1j+9JSWfYiFeqnWaKQKBgB+M\n"
    "fQzbD8g0Z+JqmAGR1Qiumt4Y48CL6QDDxvfsN9THw9MJpZiCFWEkNH6TYD01mFmE\n"
    "RwUqS0zTt/PGC+ObEZ8MMhdba0aLzJdqwN6GAIgUiCr6slFoVP5d4wjhXyyMtYVF\n"
    "kAJwvWIJKJ2T8ULUcmV1WsDiOP5lq/XjwZneardrAoGAU9+Zp2pATLKgW+wI04xj\n"
    "vPpN6rsaBAmB0P9pcRQ0ayl4bJnf2T0WQzh2z9WZZ8Gf6RIIh7dxovqVUC3j4K1W\n"
    "0nNtIGGr1sdkUio5+JAmZRaHyYriZsbP/uWjWapFeFdFrKigACDF+WNNQ1Mefgwh\n"
    "eaiAM5thamCzZz7RInDFvlM=\n"
    "-----END PRIVATE KEY-----\n";

auto CanBindLoopbackTcp() -> bool {
  try {
    asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

class FakeHttpsHubServer {
 public:
  explicit FakeHttpsHubServer(const std::filesystem::path& cert_path,
                              const std::filesystem::path& key_path)
      : cert_path_(cert_path), key_path_(key_path) {}

  ~FakeHttpsHubServer() { Stop(); }

  void Start() {
    started_.store(false);
    stop_requested_.store(false);
    thread_ = std::thread([this]() { Run(); });
    while (!started_.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  void Stop() {
    stop_requested_.store(true);
    boost::system::error_code error_code;
    if (acceptor_.has_value()) {
      acceptor_->close(error_code);
    }
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  [[nodiscard]] auto port() const -> std::uint16_t { return port_; }

  [[nodiscard]] auto WaitForRequest(std::chrono::milliseconds timeout) -> bool {
    std::unique_lock lock(mutex_);
    return cv_.wait_for(lock, timeout, [this] { return saw_request_; });
  }

 private:
  void Run() {
    try {
      asio::io_context io_context;
      ssl::context ssl_context(ssl::context::tls_server);
      ssl_context.use_certificate_chain_file(cert_path_.string());
      ssl_context.use_private_key_file(key_path_.string(), ssl::context::pem);

      tcp::acceptor acceptor(io_context, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
      acceptor.non_blocking(true);
      port_ = acceptor.local_endpoint().port();
      acceptor_.emplace(std::move(acceptor));
      started_.store(true);

      tcp::socket socket(io_context);
      while (!stop_requested_.load()) {
        boost::system::error_code accept_error;
        acceptor_->accept(socket, accept_error);
        if (!accept_error) {
          break;
        }
        if (accept_error != asio::error::would_block &&
            accept_error != asio::error::try_again &&
            accept_error != asio::error::operation_aborted) {
          return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      if (stop_requested_.load()) {
        return;
      }

      beast::ssl_stream<tcp::socket> stream(std::move(socket), ssl_context);
      boost::system::error_code error_code;
      stream.handshake(ssl::stream_base::server, error_code);
      if (error_code) {
        return;
      }

      beast::flat_buffer buffer;
      http::request<http::string_body> request;
      http::read(stream, buffer, request, error_code);
      if (error_code) {
        return;
      }

      {
        std::lock_guard lock(mutex_);
        saw_request_ = true;
      }
      cv_.notify_all();

      http::response<http::string_body> response{http::status::ok, 11};
      response.set(http::field::content_type, "application/json");
      response.body() = R"({"ok":true})";
      response.prepare_payload();
      http::write(stream, response, error_code);
      stream.shutdown(error_code);
    } catch (const std::exception&) {
      started_.store(true);
    }
  }

  std::filesystem::path cert_path_;
  std::filesystem::path key_path_;
  std::atomic<bool> started_{false};
  std::atomic<bool> stop_requested_{false};
  std::thread thread_;
  std::optional<tcp::acceptor> acceptor_;
  std::uint16_t port_{0};
  std::mutex mutex_;
  std::condition_variable cv_;
  bool saw_request_{false};
};

class HubClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_dir_ = std::filesystem::temp_directory_path() /
                ("hub-client-test-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::remove_all(temp_dir_);
    std::filesystem::create_directories(temp_dir_);
    cert_path_ = temp_dir_ / "ca.pem";
    key_path_ = temp_dir_ / "server-key.pem";
    WriteFile(cert_path_, kTestCertificatePem);
    WriteFile(key_path_, kTestPrivateKeyPem);
  }

  void TearDown() override {
    std::filesystem::remove_all(temp_dir_);
  }

  void WriteFile(const std::filesystem::path& path, const std::string_view content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(output.is_open());
    output << content;
    ASSERT_TRUE(output.good());
  }

  [[nodiscard]] auto MakeOptions() const -> HubClientOptions {
    HubClientOptions options;
    options.heartbeat_interval = std::chrono::seconds(30);
    options.request_timeout = std::chrono::milliseconds(200);
    options.use_default_verify_paths = false;
    options.ca_certificate_file = cert_path_.string();
    return options;
  }

  vibe::service::SessionManager session_manager_;
  std::filesystem::path temp_dir_;
  std::filesystem::path cert_path_;
  std::filesystem::path key_path_;
};

TEST_F(HubClientTest, TlsHeartbeatSucceedsWithTrustedCertificateForMatchingHostname) {
  if (!CanBindLoopbackTcp()) {
    GTEST_SKIP() << "loopback TCP bind is not available in this environment";
  }

  FakeHttpsHubServer server(cert_path_, key_path_);
  server.Start();

  asio::io_context io_context(1);
  auto work_guard = asio::make_work_guard(io_context);
  std::thread io_thread([&io_context]() { io_context.run(); });

  HubClient client("https://127.0.0.1:" + std::to_string(server.port()), "hub-token",
                   session_manager_, MakeOptions());
  client.Start(io_context);
  EXPECT_TRUE(server.WaitForRequest(std::chrono::seconds(2)));

  client.Stop();
  work_guard.reset();
  io_context.stop();
  io_thread.join();
  server.Stop();
}

TEST_F(HubClientTest, TlsHeartbeatFailsForWrongHostnameEvenWithTrustedCertificate) {
  if (!CanBindLoopbackTcp()) {
    GTEST_SKIP() << "loopback TCP bind is not available in this environment";
  }

  FakeHttpsHubServer server(cert_path_, key_path_);
  server.Start();

  asio::io_context io_context(1);
  auto work_guard = asio::make_work_guard(io_context);
  std::thread io_thread([&io_context]() { io_context.run(); });

  HubClient client("https://localhost:" + std::to_string(server.port()), "hub-token",
                   session_manager_, MakeOptions());
  client.Start(io_context);
  EXPECT_FALSE(server.WaitForRequest(std::chrono::milliseconds(500)));

  client.Stop();
  work_guard.reset();
  io_context.stop();
  io_thread.join();
  server.Stop();
}

TEST_F(HubClientTest, TlsHeartbeatFailsForUntrustedCertificateAuthority) {
  if (!CanBindLoopbackTcp()) {
    GTEST_SKIP() << "loopback TCP bind is not available in this environment";
  }

  FakeHttpsHubServer server(cert_path_, key_path_);
  server.Start();

  asio::io_context io_context(1);
  auto work_guard = asio::make_work_guard(io_context);
  std::thread io_thread([&io_context]() { io_context.run(); });

  HubClientOptions options = MakeOptions();
  options.ca_certificate_file = std::nullopt;

  HubClient client("https://127.0.0.1:" + std::to_string(server.port()), "hub-token",
                   session_manager_, options);
  client.Start(io_context);
  EXPECT_FALSE(server.WaitForRequest(std::chrono::milliseconds(500)));

  client.Stop();
  work_guard.reset();
  io_context.stop();
  io_thread.join();
  server.Stop();
}

TEST_F(HubClientTest, StopReturnsPromptlyWhenIoContextNeverRuns) {
  HubClientOptions options;
  options.heartbeat_interval = std::chrono::seconds(30);
  options.request_timeout = std::chrono::milliseconds(50);

  asio::io_context io_context(1);
  HubClient client("http://127.0.0.1:1", "hub-token", session_manager_, options);

  const auto started_at = std::chrono::steady_clock::now();
  client.Start(io_context);
  client.Stop();
  const auto elapsed = std::chrono::steady_clock::now() - started_at;
  EXPECT_LT(elapsed, std::chrono::seconds(1));
}

TEST_F(HubClientTest, StopIsIdempotent) {
  HubClientOptions options;
  options.heartbeat_interval = std::chrono::seconds(30);
  options.request_timeout = std::chrono::milliseconds(50);

  asio::io_context io_context(1);
  HubClient client("http://127.0.0.1:1", "hub-token", session_manager_, options);
  client.Start(io_context);
  client.Stop();
  client.Stop();
}

}  // namespace
}  // namespace vibe::net
