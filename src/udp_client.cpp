#include "opentui/udp_client.hpp"

#include <array>
#include <cstring>
#include <string>
#include <utility>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace opentui {
namespace {

#if defined(_WIN32)
using SocketType = SOCKET;
constexpr SocketType kInvalidSocket = INVALID_SOCKET;
#else
using SocketType = int;
constexpr SocketType kInvalidSocket = -1;
#endif

void close_socket(const SocketType socket_descriptor) {
#if defined(_WIN32)
  closesocket(socket_descriptor);
#else
  close(socket_descriptor);
#endif
}

[[nodiscard]] bool send_payload(const SocketType socket_descriptor, const sockaddr* destination,
                                const socklen_t destination_size, std::string_view message) {
#if defined(_WIN32)
  const int bytes_sent = sendto(socket_descriptor, message.data(), static_cast<int>(message.size()),
                                0, destination, static_cast<int>(destination_size));
#else
  const ssize_t bytes_sent =
      sendto(socket_descriptor, message.data(), message.size(), 0, destination, destination_size);
#endif

  return std::cmp_equal(bytes_sent, message.size());
}

} // namespace

#if defined(_WIN32)
class WinsockRuntime {
public:
  WinsockRuntime() {
    WSADATA data;
    initialized_ = WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }

  ~WinsockRuntime() {
    if (initialized_) {
      WSACleanup();
    }
  }

  [[nodiscard]] bool initialized() const noexcept {
    return initialized_;
  }

private:
  bool initialized_{false};
};
#endif

UdpClient::UdpClient() = default;

bool UdpClient::send_to(std::string_view host, const std::uint16_t port, std::string_view message,
                        std::string* error) const {
#if defined(_WIN32)
  static WinsockRuntime winsock_runtime;
  if (!winsock_runtime.initialized()) {
    set_error(error, "Failed to initialize WinSock.");
    return false;
  }
#endif

  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  addrinfo* results = nullptr;
  const std::string host_copy{host};
  const std::string port_string = std::to_string(port);

  const int resolve_status = getaddrinfo(host_copy.c_str(), port_string.c_str(), &hints, &results);
  if (resolve_status != 0) {
    set_error(error, "Failed to resolve host: " + host_copy);
    return false;
  }

  bool sent = false;
  for (addrinfo* candidate = results; candidate != nullptr; candidate = candidate->ai_next) {
    const SocketType socket_descriptor =
        socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
    if (socket_descriptor == kInvalidSocket) {
      continue;
    }

    sent = send_payload(socket_descriptor, candidate->ai_addr,
                        static_cast<socklen_t>(candidate->ai_addrlen), message);
    close_socket(socket_descriptor);

    if (sent) {
      break;
    }
  }

  freeaddrinfo(results);

  if (!sent) {
    set_error(error, "Failed to send UDP payload.");
  }

  return sent;
}

std::optional<std::string> UdpClient::receive_once(const std::uint16_t local_port,
                                                   const std::chrono::milliseconds timeout,
                                                   std::string* error) const {
#if defined(_WIN32)
  static WinsockRuntime winsock_runtime;
  if (!winsock_runtime.initialized()) {
    set_error(error, "Failed to initialize WinSock.");
    return std::nullopt;
  }
#endif

  const SocketType socket_descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_descriptor == kInvalidSocket) {
    set_error(error, "Failed to create UDP socket.");
    return std::nullopt;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(local_port);
  address.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(socket_descriptor, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    close_socket(socket_descriptor);
    set_error(error, "Failed to bind UDP socket.");
    return std::nullopt;
  }

#if defined(_WIN32)
  const DWORD timeout_ms = static_cast<DWORD>(timeout.count());
  setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms),
             sizeof(timeout_ms));
#else
  timeval timeout_value{};
  const auto timeout_count = timeout.count();
  timeout_value.tv_sec = static_cast<long>(timeout_count / 1000);
  timeout_value.tv_usec = static_cast<suseconds_t>((timeout_count % 1000) * 1000);
  setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout_value, sizeof(timeout_value));
#endif

  std::array<char, 2048> buffer{};
#if defined(_WIN32)
  const int bytes_received = recvfrom(socket_descriptor, buffer.data(),
                                      static_cast<int>(buffer.size()), 0, nullptr, nullptr);
#else
  const ssize_t bytes_received =
      recvfrom(socket_descriptor, buffer.data(), buffer.size(), 0, nullptr, nullptr);
#endif

  close_socket(socket_descriptor);

  if (bytes_received <= 0) {
    set_error(error, "No UDP message received before timeout.");
    return std::nullopt;
  }

  return std::string(buffer.data(), static_cast<std::size_t>(bytes_received));
}

void UdpClient::set_error(std::string* error, std::string_view message) {
  if (error != nullptr) {
    *error = std::string{message};
  }
}

} // namespace opentui
