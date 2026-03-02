#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
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
    console.println_color("Tip: /help, /status, /model, /attach, /files, /plan, ask, run, /clear",
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

              std::vector<std::string> results;
              for (const std::string_view model_name : model_candidates) {
                if (partial.empty() || model_name.starts_with(partial)) {
                  results.emplace_back(model_name);
                }
              }
              return results;
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
              std::vector<std::string> results;
              for (const std::string_view candidate : theme_candidates) {
                if (partial.empty() || candidate.starts_with(partial)) {
                  results.emplace_back(candidate);
                }
              }
              return results;
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
        .completer = nullptr,
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
        .completer = nullptr,
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
        .completer = nullptr,
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
        .completer = nullptr,
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
        .completer = nullptr,
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
