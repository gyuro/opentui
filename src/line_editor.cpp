#include "opentui/line_editor.hpp"

#include <cctype>
#include <iostream>

#if defined(_WIN32)
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace opentui {
namespace {

void redraw(std::string_view prompt, std::string_view buffer) {
  std::cout << '\r' << prompt << buffer << "\033[K" << std::flush;
}

void print_candidates(const std::vector<std::string>& candidates) {
  std::cout << '\n';
  for (const auto& candidate : candidates) {
    std::cout << candidate << "  ";
  }
  std::cout << '\n';
}

[[nodiscard]] std::string longest_common_prefix(const std::vector<std::string>& values) {
  if (values.empty()) {
    return {};
  }

  std::string prefix = values.front();
  for (std::size_t index = 1; index < values.size(); ++index) {
    const std::string& candidate = values[index];
    std::size_t shared = 0;
    while (shared < prefix.size() && shared < candidate.size() &&
           prefix[shared] == candidate[shared]) {
      ++shared;
    }

    prefix.resize(shared);
    if (prefix.empty()) {
      break;
    }
  }

  return prefix;
}

#if !defined(_WIN32)
class RawModeGuard {
public:
  RawModeGuard() {
    if (tcgetattr(STDIN_FILENO, &original_state_) != 0) {
      return;
    }

    termios raw_state = original_state_;
    raw_state.c_lflag &= static_cast<tcflag_t>(~static_cast<tcflag_t>(ICANON | ECHO));
    raw_state.c_cc[VMIN] = 1;
    raw_state.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_state) != 0) {
      return;
    }

    enabled_ = true;
  }

  ~RawModeGuard() {
    if (!enabled_) {
      return;
    }

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_state_);
  }

  [[nodiscard]] bool enabled() const noexcept {
    return enabled_;
  }

private:
  termios original_state_{};
  bool enabled_{false};
};
#endif

} // namespace

std::optional<std::string> LineEditor::read_line(std::string_view prompt,
                                                 const CompletionProvider& completion_provider) {
  const auto push_history = [this](const std::string& line) {
    if (line.empty()) {
      return;
    }

    if (!history_.empty() && history_.back() == line) {
      return;
    }

    history_.push_back(line);
    if (history_.size() > kMaxHistoryEntries) {
      history_.erase(history_.begin());
    }
  };

  if (!is_interactive()) {
    std::string line;
    if (!std::getline(std::cin, line)) {
      return std::nullopt;
    }
    push_history(line);
    return line;
  }

  std::cout << prompt << std::flush;
  std::string buffer;
  std::string draft_buffer;
  std::size_t history_index = history_.size();

  const auto move_history_up = [this, &buffer, &draft_buffer, &history_index, prompt]() {
    if (history_.empty()) {
      return false;
    }

    if (history_index == history_.size()) {
      draft_buffer = buffer;
    }

    if (history_index == 0U) {
      return false;
    }

    --history_index;
    buffer = history_[history_index];
    redraw(prompt, buffer);
    return true;
  };

  const auto move_history_down = [this, &buffer, &draft_buffer, &history_index, prompt]() {
    if (history_.empty() || history_index == history_.size()) {
      return false;
    }

    ++history_index;
    if (history_index == history_.size()) {
      buffer = draft_buffer;
    } else {
      buffer = history_[history_index];
    }

    redraw(prompt, buffer);
    return true;
  };

  const auto finalize_line = [this, &buffer, &push_history]() {
    std::cout << '\n';
    push_history(buffer);
    return std::optional<std::string>{buffer};
  };

#if defined(_WIN32)
  while (true) {
    const int key = _getch();
    if (key == 3) {
      std::cout << '\n';
      return std::nullopt;
    }

    if (key == '\r' || key == '\n') {
      return finalize_line();
    }

    if (key == '\b' || key == 127) {
      if (!buffer.empty()) {
        buffer.pop_back();
        history_index = history_.size();
        draft_buffer = buffer;
        redraw(prompt, buffer);
      }
      continue;
    }

    if (key == '\t') {
      const auto candidates = completion_provider(buffer);
      if (candidates.empty()) {
        std::cout << '\a' << std::flush;
        continue;
      }

      const std::string common_prefix = longest_common_prefix(candidates);
      if (common_prefix.size() > buffer.size()) {
        buffer = common_prefix;
        history_index = history_.size();
        draft_buffer = buffer;
        redraw(prompt, buffer);
        continue;
      }

      if (candidates.size() == 1U) {
        buffer = candidates.front();
        history_index = history_.size();
        draft_buffer = buffer;
        redraw(prompt, buffer);
        continue;
      }

      print_candidates(candidates);
      redraw(prompt, buffer);
      continue;
    }

    if (key == 0 || key == 224) {
      const int special_key = _getch();
      if (special_key == 72) {
        if (!move_history_up()) {
          std::cout << '\a' << std::flush;
        }
        continue;
      }

      if (special_key == 80) {
        if (!move_history_down()) {
          std::cout << '\a' << std::flush;
        }
        continue;
      }
      continue;
    }

    if (std::isprint(key) != 0) {
      buffer.push_back(static_cast<char>(key));
      history_index = history_.size();
      draft_buffer = buffer;
      redraw(prompt, buffer);
    }
  }
#else
  RawModeGuard raw_mode;
  if (!raw_mode.enabled()) {
    std::string line;
    if (!std::getline(std::cin, line)) {
      return std::nullopt;
    }
    push_history(line);
    return line;
  }

  while (true) {
    char key = '\0';
    if (read(STDIN_FILENO, &key, 1) != 1) {
      return std::nullopt;
    }

    if (key == 4 && buffer.empty()) {
      std::cout << '\n';
      return std::nullopt;
    }

    if (key == '\r' || key == '\n') {
      return finalize_line();
    }

    if (key == '\b' || key == 127) {
      if (!buffer.empty()) {
        buffer.pop_back();
        history_index = history_.size();
        draft_buffer = buffer;
        redraw(prompt, buffer);
      }
      continue;
    }

    if (key == '\t') {
      const auto candidates = completion_provider(buffer);
      if (candidates.empty()) {
        std::cout << '\a' << std::flush;
        continue;
      }

      const std::string common_prefix = longest_common_prefix(candidates);
      if (common_prefix.size() > buffer.size()) {
        buffer = common_prefix;
        history_index = history_.size();
        draft_buffer = buffer;
        redraw(prompt, buffer);
        continue;
      }

      if (candidates.size() == 1U) {
        buffer = candidates.front();
        history_index = history_.size();
        draft_buffer = buffer;
        redraw(prompt, buffer);
        continue;
      }

      print_candidates(candidates);
      redraw(prompt, buffer);
      continue;
    }

    if (key == '\033') {
      char next = '\0';
      if (read(STDIN_FILENO, &next, 1) != 1 || next != '[') {
        continue;
      }

      char arrow = '\0';
      if (read(STDIN_FILENO, &arrow, 1) != 1) {
        continue;
      }

      if (arrow == 'A') {
        if (!move_history_up()) {
          std::cout << '\a' << std::flush;
        }
        continue;
      }

      if (arrow == 'B') {
        if (!move_history_down()) {
          std::cout << '\a' << std::flush;
        }
        continue;
      }

      continue;
    }

    if (std::isprint(static_cast<unsigned char>(key)) != 0) {
      buffer.push_back(key);
      history_index = history_.size();
      draft_buffer = buffer;
      redraw(prompt, buffer);
    }
  }
#endif
}

bool LineEditor::is_interactive() {
#if defined(_WIN32)
  return _isatty(_fileno(stdin)) != 0 && _isatty(_fileno(stdout)) != 0;
#else
  return isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
#endif
}

} // namespace opentui
