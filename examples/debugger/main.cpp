#include <algorithm>
#include <charconv>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include "opentui/tui_application.hpp"
#include "opentui/udp_client.hpp"

namespace {

[[nodiscard]] std::optional<int> parse_int(std::string_view text) {
  int value = 0;
  const auto* begin = text.data();
  const auto* end = begin + text.size();
  const auto [pointer, error] = std::from_chars(begin, end, value);
  if (error != std::errc{} || pointer != end) {
    return std::nullopt;
  }
  return value;
}

class DebuggerApp final : public opentui::TuiApplication {
protected:
  [[nodiscard]] std::string banner() const override {
    return "open tui c++ | sample debugger";
  }

  [[nodiscard]] std::string prompt() const override {
    return "dbg> ";
  }

  void on_start(opentui::Console& console) override {
    console.println_color("Type 'help' to list commands.", opentui::Color::BrightBlack);
  }

  void register_commands(opentui::CommandRegistry& registry) override {
    const auto register_command = [this, &registry](opentui::Command command) {
      const std::string command_name = command.name;
      if (!registry.add(std::move(command))) {
        console().println_color("Failed to register command: " + command_name,
                                opentui::Color::BrightRed);
      }
    };

    register_command(opentui::Command{
        .name = "status",
        .description = "Show debugger state.",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(args);
              static_cast<void>(context);
              const std::string trace_state = tracing_enabled_ ? "on" : "off";
              console().println_color("program_counter=" + std::to_string(program_counter_),
                                      opentui::Color::BrightGreen);
              console().println_color("trace=" + trace_state, opentui::Color::BrightGreen);
            },
        .completer = nullptr,
    });

    register_command(opentui::Command{
        .name = "step",
        .description = "Increment program counter by N (default: 1).",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(context);

              int increment = 1;
              if (!args.empty()) {
                const auto parsed = parse_int(args.front());
                if (!parsed.has_value() || *parsed <= 0) {
                  console().println_color("Usage: step [positive_integer]",
                                          opentui::Color::BrightRed);
                  return;
                }
                increment = *parsed;
              }

              program_counter_ += increment;
              console().println_color("Stepped to " + std::to_string(program_counter_),
                                      opentui::Color::BrightCyan);
            },
        .completer = nullptr,
    });

    register_command(opentui::Command{
        .name = "trace",
        .description = "Set trace mode: on|off.",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(context);

              if (args.size() != 1U) {
                console().println_color("Usage: trace <on|off>", opentui::Color::BrightRed);
                return;
              }

              if (args.front() == "on") {
                tracing_enabled_ = true;
                console().println_color("Trace enabled.", opentui::Color::BrightYellow);
                return;
              }

              if (args.front() == "off") {
                tracing_enabled_ = false;
                console().println_color("Trace disabled.", opentui::Color::BrightYellow);
                return;
              }

              console().println_color("Usage: trace <on|off>", opentui::Color::BrightRed);
            },
        .completer =
            [](const std::string_view partial, const opentui::Args& args) {
              if (!args.empty()) {
                return std::vector<std::string>{};
              }

              std::vector<std::string> options{"on", "off"};
              if (partial.empty()) {
                return options;
              }

              std::vector<std::string> filtered;
              std::ranges::copy_if(
                  options, std::back_inserter(filtered),
                  [partial](std::string_view option) { return option.starts_with(partial); });
              return filtered;
            },
    });

    register_command(opentui::Command{
        .name = "udp_send",
        .description = "Send UDP message: udp_send <host> <port> <message>",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(context);

              if (args.size() < 3U) {
                console().println_color("Usage: udp_send <host> <port> <message>",
                                        opentui::Color::BrightRed);
                return;
              }

              const std::string_view host = args[0];
              const auto parsed_port = parse_int(args[1]);
              if (!parsed_port.has_value() || *parsed_port <= 0 || *parsed_port > 65535) {
                console().println_color("Invalid UDP port.", opentui::Color::BrightRed);
                return;
              }

              std::string payload;
              for (std::size_t index = 2; index < args.size(); ++index) {
                if (index != 2U) {
                  payload += ' ';
                }
                payload += args[index];
              }

              std::string error;
              const bool sent = udp_client_.send_to(host, static_cast<std::uint16_t>(*parsed_port),
                                                    payload, &error);

              if (!sent) {
                console().println_color("UDP send failed: " + error, opentui::Color::BrightRed);
                return;
              }

              console().println_color("UDP payload sent.", opentui::Color::BrightGreen);
            },
        .completer = nullptr,
    });

    register_command(opentui::Command{
        .name = "udp_wait",
        .description = "Wait for UDP packet: udp_wait <port> [timeout_ms]",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(context);

              if (args.empty() || args.size() > 2U) {
                console().println_color("Usage: udp_wait <port> [timeout_ms]",
                                        opentui::Color::BrightRed);
                return;
              }

              const auto parsed_port = parse_int(args.front());
              if (!parsed_port.has_value() || *parsed_port <= 0 || *parsed_port > 65535) {
                console().println_color("Invalid UDP port.", opentui::Color::BrightRed);
                return;
              }

              int timeout_ms = 3000;
              if (args.size() == 2U) {
                const auto parsed_timeout = parse_int(args[1]);
                if (!parsed_timeout.has_value() || *parsed_timeout < 0) {
                  console().println_color("Invalid timeout value.", opentui::Color::BrightRed);
                  return;
                }
                timeout_ms = *parsed_timeout;
              }

              std::string error;
              const auto message =
                  udp_client_.receive_once(static_cast<std::uint16_t>(*parsed_port),
                                           std::chrono::milliseconds(timeout_ms), &error);

              if (!message.has_value()) {
                console().println_color("UDP wait failed: " + error, opentui::Color::BrightRed);
                return;
              }

              console().println_color("Received: " + *message, opentui::Color::BrightGreen);
            },
        .completer = nullptr,
    });
  }

private:
  int program_counter_{0};
  bool tracing_enabled_{false};
  opentui::UdpClient udp_client_;
};

} // namespace

int main() {
  DebuggerApp app;
  return app.run();
}
