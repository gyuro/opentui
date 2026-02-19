#include "opentui/tui_application.hpp"

#include <string_view>
#include <utility>

#include "opentui/signal_manager.hpp"

namespace opentui {

namespace {

std::vector<std::string> no_completion(std::string_view partial, const Args& args) {
  static_cast<void>(partial);
  static_cast<void>(args);
  return {};
}

} // namespace

std::string TuiApplication::banner() const {
  return "open tui c++";
}

std::string TuiApplication::prompt() const {
  return "tui> ";
}

void TuiApplication::on_start(Console& console) {
  static_cast<void>(console);
}

void TuiApplication::on_shutdown(Console& console) {
  static_cast<void>(console);
}

Console& TuiApplication::console() noexcept {
  return console_;
}

int TuiApplication::run() {
  command_registry_ = CommandRegistry{};
  running_.store(true);

  SignalManager signal_manager;
  register_builtin_commands();
  register_commands(command_registry_);

  console_.println_color(banner(), Color::BrightCyan, Color::Default, true);
  on_start(console_);

  CommandContext context{.console = console_, .running = running_};

  while (running_.load() && !signal_manager.stop_requested()) {
    const auto line = line_editor_.read_line(prompt(), [this](const std::string_view input_buffer) {
      return command_registry_.complete(input_buffer);
    });

    if (!line.has_value()) {
      break;
    }

    command_registry_.execute_line(*line, context);
  }

  if (signal_manager.stop_requested()) {
    console_.println_color("Termination signal received. Exiting...", Color::BrightYellow);
  }

  on_shutdown(console_);
  return 0;
}

void TuiApplication::register_builtin_commands() {
  const auto register_builtin = [this](Command command) {
    const std::string command_name = command.name;
    if (!command_registry_.add(std::move(command))) {
      console_.println_color("Failed to register builtin command: " + command_name,
                             Color::BrightRed);
    }
  };

  register_builtin(Command{
      .name = "help",
      .description = "Show all available commands.",
      .handler =
          [this](const Args& args, CommandContext& context) {
            static_cast<void>(args);
            static_cast<void>(context);
            console_.println(command_registry_.help_text());
          },
      .completer = no_completion,
  });

  const auto exit_handler = [](const Args& args, CommandContext& context) {
    static_cast<void>(args);
    context.running.store(false);
  };

  register_builtin(Command{
      .name = "exit",
      .description = "Exit the debugger interface.",
      .handler = exit_handler,
      .completer = no_completion,
  });

  register_builtin(Command{
      .name = "quit",
      .description = "Alias for exit.",
      .handler = exit_handler,
      .completer = no_completion,
  });
}

} // namespace opentui
