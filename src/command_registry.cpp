#include "opentui/command_registry.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <ranges>
#include <sstream>

#include "opentui/console.hpp"

namespace opentui {
namespace {

[[nodiscard]] std::vector<std::string> split_for_completion(std::string_view line) {
  std::vector<std::string> parts;
  std::string current;

  for (const char c : line) {
    if (std::isspace(static_cast<unsigned char>(c)) != 0) {
      if (!current.empty()) {
        parts.push_back(current);
        current.clear();
      }
      continue;
    }
    current.push_back(c);
  }

  if (!current.empty()) {
    parts.push_back(current);
  }

  return parts;
}

[[nodiscard]] std::string join_with_spaces(const Args& values) {
  std::ostringstream output;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0U) {
      output << ' ';
    }
    output << values[index];
  }
  return output.str();
}

[[nodiscard]] std::string join_with_commas(const std::vector<std::string>& values) {
  std::ostringstream output;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0U) {
      output << ", ";
    }
    output << values[index];
  }
  return output.str();
}

} // namespace

bool CommandRegistry::add(Command command) {
  if (command.name.empty() || !command.handler) {
    return false;
  }

  auto [_, inserted] = commands_.emplace(command.name, std::move(command));
  return inserted;
}

bool CommandRegistry::contains(std::string_view name) const {
  return commands_.contains(std::string{name});
}

std::optional<std::reference_wrapper<const Command>>
CommandRegistry::find(std::string_view name) const {
  const auto iterator = commands_.find(std::string{name});
  if (iterator == commands_.end()) {
    return std::nullopt;
  }
  return std::cref(iterator->second);
}

std::vector<std::string> CommandRegistry::names() const {
  std::vector<std::string> values;
  values.reserve(commands_.size());
  for (const auto& [name, _] : commands_) {
    values.push_back(name);
  }
  return values;
}

std::vector<std::string> CommandRegistry::complete(std::string_view buffer) const {
  std::vector<std::string> completions;

  const bool trailing_space =
      !buffer.empty() && std::isspace(static_cast<unsigned char>(buffer.back())) != 0;
  const std::vector<std::string> tokens = split_for_completion(buffer);

  if (tokens.empty()) {
    const auto command_names = names();
    completions.reserve(command_names.size());
    for (const auto& command_name : command_names) {
      completions.push_back(command_name + " ");
    }
    return completions;
  }

  if (tokens.size() == 1U && !trailing_space) {
    for (const auto& [name, _] : commands_) {
      if (std::string_view{name}.starts_with(tokens.front())) {
        completions.push_back(name + " ");
      }
    }
    return completions;
  }

  const std::string_view command_name = tokens.front();
  const auto command = find(command_name);
  if (!command.has_value() || !command->get().completer) {
    return completions;
  }

  Args stable_args;
  std::string partial;

  if (trailing_space) {
    if (tokens.size() > 1U) {
      stable_args.assign(tokens.begin() + 1, tokens.end());
    }
  } else {
    if (tokens.size() > 2U) {
      stable_args.assign(tokens.begin() + 1, tokens.end() - 1);
    }
    partial = tokens.back();
  }

  const auto suggestions = command->get().completer(partial, stable_args);
  if (suggestions.empty()) {
    return completions;
  }

  std::string prefix{command_name};
  prefix += ' ';
  if (!stable_args.empty()) {
    prefix += join_with_spaces(stable_args);
    prefix += ' ';
  }

  completions.reserve(suggestions.size());
  for (const auto& suggestion : suggestions) {
    completions.push_back(prefix + suggestion);
  }

  std::ranges::sort(completions);
  const auto unique_result = std::ranges::unique(completions);
  completions.erase(unique_result.begin(), completions.end());
  return completions;
}

std::string CommandRegistry::help_text() const {
  std::ostringstream output;
  output << "Available commands:\n";

  std::size_t max_name_width = 0;
  for (const auto& [name, _] : commands_) {
    max_name_width = std::max(max_name_width, name.size());
  }

  for (const auto& [name, command] : commands_) {
    output << "  " << std::left << std::setw(static_cast<int>(max_name_width)) << name << "  "
           << command.description << '\n';
  }

  return output.str();
}

bool CommandRegistry::execute_line(std::string_view line, CommandContext& context) const {
  const Args tokens = tokenize(line);
  if (tokens.empty()) {
    return true;
  }

  const auto command = find(tokens.front());
  if (!command.has_value()) {
    context.console.println_color("Unknown command: " + tokens.front(), Color::BrightRed);

    std::vector<std::string> suggestions;
    for (const auto& [name, _] : commands_) {
      if (std::string_view{name}.starts_with(tokens.front())) {
        suggestions.push_back(name);
      }
    }

    if (!suggestions.empty()) {
      context.console.println("Possible matches: " + join_with_commas(suggestions));
    }

    return false;
  }

  Args args;
  if (tokens.size() > 1U) {
    args.assign(tokens.begin() + 1, tokens.end());
  }

  command->get().handler(args, context);
  return true;
}

Args CommandRegistry::tokenize(std::string_view line) {
  Args tokens;
  std::string current;
  char quote = '\0';

  for (std::size_t index = 0; index < line.size(); ++index) {
    const char character = line[index];

    if (character == '\\' && (index + 1U) < line.size()) {
      current.push_back(line[index + 1U]);
      ++index;
      continue;
    }

    if (quote != '\0') {
      if (character == quote) {
        quote = '\0';
      } else {
        current.push_back(character);
      }
      continue;
    }

    if (character == '"' || character == '\'') {
      quote = character;
      continue;
    }

    if (std::isspace(static_cast<unsigned char>(character)) != 0) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }

    current.push_back(character);
  }

  if (!current.empty()) {
    tokens.push_back(current);
  }

  return tokens;
}

} // namespace opentui
