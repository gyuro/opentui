#include "opentui/console.hpp"

#include <iostream>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace opentui {

Console::Console() : ansi_enabled_(enable_virtual_terminal()) {}

void Console::print(std::string_view text) {
  std::cout << text;
}

void Console::println(std::string_view text) {
  std::cout << text << '\n';
}

void Console::print_color(std::string_view text, const Color foreground, const Color background,
                          const bool bold) {
  std::cout << paint(text, foreground, background, bold);
}

void Console::println_color(std::string_view text, const Color foreground, const Color background,
                            const bool bold) {
  std::cout << paint(text, foreground, background, bold) << '\n';
}

std::string Console::paint(std::string_view text, const Color foreground, const Color background,
                           const bool bold) const {
  if (!ansi_enabled_) {
    return std::string{text};
  }

  std::vector<int> codes;
  if (bold) {
    codes.push_back(1);
  }

  const int foreground_code = ansi_foreground(foreground);
  const int background_code = ansi_background(background);

  if (foreground_code >= 0) {
    codes.push_back(foreground_code);
  }
  if (background_code >= 0) {
    codes.push_back(background_code);
  }

  if (codes.empty()) {
    return std::string{text};
  }

  std::ostringstream output;
  output << "\033[";
  for (std::size_t index = 0; index < codes.size(); ++index) {
    if (index != 0U) {
      output << ';';
    }
    output << codes[index];
  }
  output << 'm' << text << "\033[0m";
  return output.str();
}

void Console::flush() {
  std::cout << std::flush;
}

bool Console::enable_virtual_terminal() {
#if defined(_WIN32)
  const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
  if (output == INVALID_HANDLE_VALUE) {
    return false;
  }

  DWORD mode = 0;
  if (GetConsoleMode(output, &mode) == 0) {
    return false;
  }

  constexpr DWORD kAnsiFlag = ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if ((mode & kAnsiFlag) == 0U) {
    const DWORD updated_mode = mode | kAnsiFlag;
    if (SetConsoleMode(output, updated_mode) == 0) {
      return false;
    }
  }
  return true;
#else
  return isatty(STDOUT_FILENO) != 0;
#endif
}

int Console::ansi_foreground(const Color color) {
  const int value = static_cast<int>(color);
  if (value < 0) {
    return -1;
  }
  if (value <= 7) {
    return 30 + value;
  }
  if (value <= 15) {
    return 90 + (value - 8);
  }
  return -1;
}

int Console::ansi_background(const Color color) {
  const int value = static_cast<int>(color);
  if (value < 0) {
    return -1;
  }
  if (value <= 7) {
    return 40 + value;
  }
  if (value <= 15) {
    return 100 + (value - 8);
  }
  return -1;
}

} // namespace opentui
