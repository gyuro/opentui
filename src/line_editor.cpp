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

#if !defined(_WIN32)
class RawModeGuard {
public:
  RawModeGuard() {
    if (tcgetattr(STDIN_FILENO, &original_state_) != 0) {
      return;
    }

    termios raw_state = original_state_;
    raw_state.c_lflag &= static_cast<unsigned long>(~(ICANON | ECHO));
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
  if (!is_interactive()) {
    std::string line;
    if (!std::getline(std::cin, line)) {
      return std::nullopt;
    }
    return line;
  }

  std::cout << prompt << std::flush;
  std::string buffer;

#if defined(_WIN32)
  while (true) {
    const int key = _getch();
    if (key == 3) {
      std::cout << '\n';
      return std::nullopt;
    }

    if (key == '\r' || key == '\n') {
      std::cout << '\n';
      return buffer;
    }

    if (key == '\b' || key == 127) {
      if (!buffer.empty()) {
        buffer.pop_back();
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

      if (candidates.size() == 1U) {
        buffer = candidates.front();
        redraw(prompt, buffer);
        continue;
      }

      print_candidates(candidates);
      redraw(prompt, buffer);
      continue;
    }

    if (key == 0 || key == 224) {
      const int ignored = _getch();
      static_cast<void>(ignored);
      continue;
    }

    if (std::isprint(key) != 0) {
      buffer.push_back(static_cast<char>(key));
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
      std::cout << '\n';
      return buffer;
    }

    if (key == '\b' || key == 127) {
      if (!buffer.empty()) {
        buffer.pop_back();
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

      if (candidates.size() == 1U) {
        buffer = candidates.front();
        redraw(prompt, buffer);
        continue;
      }

      print_candidates(candidates);
      redraw(prompt, buffer);
      continue;
    }

    if (std::isprint(static_cast<unsigned char>(key)) != 0) {
      buffer.push_back(key);
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
