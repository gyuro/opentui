#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opentui {

class Console;

using Args = std::vector<std::string>;

struct CommandContext {
  Console& console;
  std::atomic_bool& running;
};

using CommandHandler = std::function<void(const Args& args, CommandContext& context)>;
using CompletionHandler =
    std::function<std::vector<std::string>(std::string_view partial, const Args& args)>;

struct Command {
  std::string name;
  std::string description;
  CommandHandler handler;
  CompletionHandler completer;
};

class CommandRegistry {
public:
  [[nodiscard]] bool add(Command command);
  [[nodiscard]] bool contains(std::string_view name) const;
  [[nodiscard]] std::optional<std::reference_wrapper<const Command>>
  find(std::string_view name) const;
  [[nodiscard]] std::vector<std::string> names() const;
  [[nodiscard]] std::vector<std::string> complete(std::string_view buffer) const;
  [[nodiscard]] std::string help_text() const;

  bool execute_line(std::string_view line, CommandContext& context) const;

private:
  [[nodiscard]] static Args tokenize(std::string_view line);

  std::map<std::string, Command, std::less<>> commands_;
};

} // namespace opentui
