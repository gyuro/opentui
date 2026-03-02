#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "opentui/tui_application.hpp"

namespace {

constexpr std::size_t kPanelWidth = 92;

[[nodiscard]] std::string make_border() {
  return "+" + std::string(kPanelWidth - 2U, '-') + "+";
}

[[nodiscard]] std::string fit_cell(std::string text, const std::size_t width) {
  if (text.size() > width) {
    if (width > 3U) {
      text = text.substr(0, width - 3U) + "...";
    } else {
      text = text.substr(0, width);
    }
  }

  if (text.size() < width) {
    text.append(width - text.size(), ' ');
  }

  return text;
}

[[nodiscard]] std::string panel_line(const std::string& body) {
  return "| " + fit_cell(body, kPanelWidth - 4U) + " |";
}

[[nodiscard]] std::string join_args(const opentui::Args& args, const std::size_t start_index = 0U) {
  std::string joined;
  for (std::size_t index = start_index; index < args.size(); ++index) {
    if (index != start_index) {
      joined.push_back(' ');
    }
    joined += args[index];
  }
  return joined;
}

[[nodiscard]] std::size_t estimated_tokens_for(const std::string_view text) {
  const std::size_t minimum = 8U;
  const std::size_t estimated = (text.size() / 3U) + 1U;
  return std::max(minimum, estimated);
}

template <std::size_t N>
[[nodiscard]] std::vector<std::string>
prefix_filter(const std::string_view partial, const std::array<std::string_view, N>& candidates) {
  std::vector<std::string> results;
  for (const std::string_view candidate : candidates) {
    if (partial.empty() || candidate.starts_with(partial)) {
      results.emplace_back(candidate);
    }
  }
  return results;
}

[[nodiscard]] std::vector<std::string> complete_path_argument(const std::string_view partial) {
  namespace fs = std::filesystem;

  std::string input{partial};
  if (input == "~") {
    input = "~/";
  }

  std::string display_base;
  std::string leaf_prefix;

  const auto split_input = [&](const std::string& text) {
    if (!text.empty() && (text.back() == '/' || text.back() == '\\')) {
      display_base = text;
      leaf_prefix.clear();
      return;
    }

    const std::size_t separator_pos = text.find_last_of("/\\");
    if (separator_pos == std::string::npos) {
      display_base.clear();
      leaf_prefix = text;
      return;
    }

    display_base = text.substr(0, separator_pos + 1U);
    leaf_prefix = text.substr(separator_pos + 1U);
  };

  split_input(input);

  fs::path resolved_directory{"."};
  if (display_base.rfind("~/", 0U) == 0U) {
    const char* home = std::getenv("HOME");
    if (home == nullptr) {
      return {};
    }
    resolved_directory = fs::path(home) / display_base.substr(2U);
  } else if (!display_base.empty()) {
    resolved_directory = fs::path(display_base);
  }

  std::error_code error;
  if (!fs::exists(resolved_directory, error) || !fs::is_directory(resolved_directory, error)) {
    return {};
  }

  std::vector<std::string> suggestions;
  for (const fs::directory_entry& entry : fs::directory_iterator(
           resolved_directory, fs::directory_options::skip_permission_denied, error)) {
    if (error) {
      break;
    }

    const std::string file_name = entry.path().filename().string();
    if (!leaf_prefix.empty() && !std::string_view{file_name}.starts_with(leaf_prefix)) {
      continue;
    }

    std::error_code type_error;
    const bool is_directory = entry.is_directory(type_error);

    std::string suggestion = display_base + file_name;
    if (!type_error && is_directory) {
      suggestion.push_back('/');
    }
    suggestions.push_back(std::move(suggestion));
  }

  std::ranges::sort(suggestions);
  const auto unique_result = std::ranges::unique(suggestions);
  suggestions.erase(unique_result.begin(), suggestions.end());
  return suggestions;
}

class ClaudeCodeStyleDemo final : public opentui::TuiApplication {
protected:
  [[nodiscard]] std::string banner() const override {
    return "open tui c++ | Claude Code-style workspace demo";
  }

  [[nodiscard]] std::string prompt() const override {
    return "claude> ";
  }

  void on_start(opentui::Console& console) override {
    render_shell_chrome(console);
    console.println_color(
        "Tip: /help, /status, /model, /attach, /files, /plan, ask, run, /clear "
        "(live list below input, Tab=autocomplete, Right=accept autosuggest, Up/Down=history)",
        opentui::Color::BrightBlack);
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
        .name = "/status",
        .description = "Render Claude Code-style shell chrome and session stats.",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(args);
              static_cast<void>(context);
              render_shell_chrome(console());
            },
        .completer = nullptr,
    });

    register_command(opentui::Command{
        .name = "/model",
        .description = "Set or show current model. Usage: /model <name>",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(context);

              if (args.empty()) {
                console().println_color("Current model: " + model_, opentui::Color::BrightGreen);
                return;
              }

              if (args.size() != 1U) {
                console().println_color("Usage: /model <name>", opentui::Color::BrightRed);
                return;
              }

              model_ = args.front();
              console().println_color("Model switched to " + model_, opentui::Color::BrightGreen);
            },
        .completer =
            [](const std::string_view partial, const opentui::Args& args) {
              if (!args.empty()) {
                return std::vector<std::string>{};
              }

              constexpr std::array<std::string_view, 4> model_candidates{
                  "claude-haiku-3.5", "claude-sonnet-4.5", "claude-opus-4", "gpt-5-codex"};
              return prefix_filter(partial, model_candidates);
            },
    });

    register_command(opentui::Command{
        .name = "/theme",
        .description = "Set or show theme. Usage: /theme <dark|dusk|light>",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(context);

              if (args.empty()) {
                console().println_color("Current theme: " + theme_, opentui::Color::BrightGreen);
                return;
              }

              if (args.size() != 1U) {
                console().println_color("Usage: /theme <dark|dusk|light>",
                                        opentui::Color::BrightRed);
                return;
              }

              const std::string_view value = args.front();
              if (value != "dark" && value != "dusk" && value != "light") {
                console().println_color("Usage: /theme <dark|dusk|light>",
                                        opentui::Color::BrightRed);
                return;
              }

              theme_ = std::string{value};
              console().println_color("Theme switched to " + theme_, opentui::Color::BrightGreen);
            },
        .completer =
            [](const std::string_view partial, const opentui::Args& args) {
              if (!args.empty()) {
                return std::vector<std::string>{};
              }

              constexpr std::array<std::string_view, 3> theme_candidates{"dark", "dusk", "light"};
              return prefix_filter(partial, theme_candidates);
            },
    });

    register_command(opentui::Command{
        .name = "/attach",
        .description = "Attach a context file path. Usage: /attach <path>",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(context);

              if (args.size() != 1U) {
                console().println_color("Usage: /attach <path>", opentui::Color::BrightRed);
                return;
              }

              const std::string& path = args.front();
              if (std::ranges::find(attached_files_, path) != attached_files_.end()) {
                console().println_color("Already attached: " + path, opentui::Color::BrightYellow);
                return;
              }

              attached_files_.push_back(path);
              console().println_color("Attached: " + path, opentui::Color::BrightGreen);
            },
        .completer =
            [](const std::string_view partial, const opentui::Args& args) {
              if (!args.empty()) {
                return std::vector<std::string>{};
              }
              return complete_path_argument(partial);
            },
    });

    register_command(opentui::Command{
        .name = "/files",
        .description = "List attached context files.",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(args);
              static_cast<void>(context);

              if (attached_files_.empty()) {
                console().println_color("No context files attached.", opentui::Color::BrightBlack);
                return;
              }

              console().println_color("Attached context files:", opentui::Color::BrightCyan,
                                      opentui::Color::Default, true);
              for (const auto& file_path : attached_files_) {
                console().println("  - " + file_path);
              }
            },
        .completer = nullptr,
    });

    register_command(opentui::Command{
        .name = "/focus",
        .description = "Set current focus area. Usage: /focus <topic>",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(context);

              if (args.empty()) {
                console().println_color("Usage: /focus <topic>", opentui::Color::BrightRed);
                return;
              }

              focus_ = join_args(args);
              console().println_color("Focus set to: " + focus_, opentui::Color::BrightGreen);
            },
        .completer =
            [](const std::string_view partial, const opentui::Args& args) {
              if (!args.empty()) {
                return std::vector<std::string>{};
              }

              constexpr std::array<std::string_view, 7> focus_candidates{
                  "code", "tests", "ci", "docs", "performance", "refactor", "release"};
              return prefix_filter(partial, focus_candidates);
            },
    });

    register_command(opentui::Command{
        .name = "/plan",
        .description = "Generate a short execution plan. Usage: /plan <task>",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(context);

              if (args.empty()) {
                console().println_color("Usage: /plan <task>", opentui::Color::BrightRed);
                return;
              }

              const std::string task = join_args(args);
              token_estimate_ += estimated_tokens_for(task) + 12U;

              console().println_color("plan > " + task, opentui::Color::BrightMagenta);
              console().println("  1) Read current files and constraints.");
              console().println("  2) Apply smallest safe edits with quick verification.");
              console().println("  3) Summarize changes + next optional refinements.");
            },
        .completer =
            [](const std::string_view partial, const opentui::Args& args) {
              if (!args.empty()) {
                return std::vector<std::string>{};
              }

              constexpr std::array<std::string_view, 7> plan_candidates{
                  "implement",    "refactor", "debug", "benchmark",
                  "stabilize-ci", "document", "ship"};
              return prefix_filter(partial, plan_candidates);
            },
    });

    register_command(opentui::Command{
        .name = "ask",
        .description = "Send a prompt and print a structured assistant response.",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(context);

              if (args.empty()) {
                console().println_color("Usage: ask <message>", opentui::Color::BrightRed);
                return;
              }

              const std::string message = join_args(args);
              token_estimate_ += estimated_tokens_for(message) + 18U;

              console().println_color("user > " + message, opentui::Color::BrightWhite);
              console().println_color("assistant >", opentui::Color::BrightCyan,
                                      opentui::Color::Default, true);
              console().println("  - I can implement this with a small, testable patch.");
              console().println("  - I will keep the scope tight and preserve existing behavior.");
              console().println("  - After patching, I will run a focused verification step.");
            },
        .completer =
            [](const std::string_view partial, const opentui::Args& args) {
              if (!args.empty()) {
                return std::vector<std::string>{};
              }

              constexpr std::array<std::string_view, 4> ask_starters{"can you", "please",
                                                                     "how do I", "why did"};
              return prefix_filter(partial, ask_starters);
            },
    });

    register_command(opentui::Command{
        .name = "run",
        .description = "Simulate a tool execution block. Usage: run <command>",
        .handler =
            [this](const opentui::Args& args, opentui::CommandContext& context) {
              static_cast<void>(context);

              if (args.empty()) {
                console().println_color("Usage: run <command>", opentui::Color::BrightRed);
                return;
              }

              const std::string command = join_args(args);
              token_estimate_ += estimated_tokens_for(command) + 9U;

              console().println_color("tool > " + command, opentui::Color::BrightBlue);
              console().println_color("result > success (simulated)", opentui::Color::BrightGreen);
            },
        .completer =
            [](const std::string_view partial, const opentui::Args& args) {
              if (!args.empty()) {
                return std::vector<std::string>{};
              }

              constexpr std::array<std::string_view, 7> run_candidates{
                  "cmake -S . -B build",
                  "cmake --build build",
                  "ctest --test-dir build",
                  "git status",
                  "git diff",
                  "./scripts/tasks.sh all",
                  "./scripts/tasks.sh run-claude-example"};
              return prefix_filter(partial, run_candidates);
            },
    });
  }

private:
  void render_shell_chrome(opentui::Console& console) const {
    const std::string top_line = make_border();
    const std::string title_line = panel_line("Claude Code-style TUI | model=" + model_ +
                                              " | theme=" + theme_ + " | focus=" + focus_);
    const std::string status_line =
        panel_line("context_files=" + std::to_string(attached_files_.size()) +
                   " | token_estimate=" + std::to_string(token_estimate_) +
                   " | slash commands + history navigation enabled");
    const std::string footer_line = panel_line(
        "workflow: /plan <task> -> run <cmd> -> ask <message> -> /status (screen clear: /clear)");

    console.println_color(top_line, opentui::Color::BrightBlack);
    console.println_color(title_line, opentui::Color::BrightCyan, opentui::Color::Default, true);
    console.println_color(status_line, opentui::Color::BrightBlack);
    console.println_color(footer_line, opentui::Color::BrightBlack);
    console.println_color(top_line, opentui::Color::BrightBlack);
  }

  std::string model_ = "claude-sonnet-4.5";
  std::string theme_ = "dark";
  std::string focus_ = "code edits";
  std::vector<std::string> attached_files_;
  std::size_t token_estimate_{0U};
};

} // namespace

int main() {
  ClaudeCodeStyleDemo app;
  return app.run();
}
