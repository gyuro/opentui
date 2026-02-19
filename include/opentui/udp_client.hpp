#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace opentui {

class UdpClient {
public:
  UdpClient();
  ~UdpClient() = default;

  UdpClient(const UdpClient&) = delete;
  UdpClient& operator=(const UdpClient&) = delete;

  [[nodiscard]] bool send_to(std::string_view host, std::uint16_t port, std::string_view message,
                             std::string* error = nullptr) const;

  [[nodiscard]] std::optional<std::string> receive_once(std::uint16_t local_port,
                                                        std::chrono::milliseconds timeout,
                                                        std::string* error = nullptr) const;

private:
  static void set_error(std::string* error, std::string_view message);
};

} // namespace opentui
