#pragma once

#include <atomic>
#include <string>

#include "opentui/command_registry.hpp"
#include "opentui/console.hpp"
#include "opentui/line_editor.hpp"

namespace opentui {

class TuiApplication {
public:
  virtual ~TuiApplication() = default;

  int run();

protected:
  [[nodiscard]] virtual std::string banner() const;
  [[nodiscard]] virtual std::string prompt() const;

  virtual void on_start(Console& console);
  virtual void on_shutdown(Console& console);

  virtual void register_commands(CommandRegistry& registry) = 0;

  Console& console() noexcept;

private:
  void register_builtin_commands();

  CommandRegistry command_registry_;
  Console console_;
  LineEditor line_editor_;
  std::atomic_bool running_{true};
};

} // namespace opentui
