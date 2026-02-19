#pragma once

#include <string>
#include <string_view>

namespace opentui {

enum class Color {
  Default = -1,
  Black = 0,
  Red = 1,
  Green = 2,
  Yellow = 3,
  Blue = 4,
  Magenta = 5,
  Cyan = 6,
  White = 7,
  BrightBlack = 8,
  BrightRed = 9,
  BrightGreen = 10,
  BrightYellow = 11,
  BrightBlue = 12,
  BrightMagenta = 13,
  BrightCyan = 14,
  BrightWhite = 15,
};

class Console {
public:
  Console();

  void print(std::string_view text);
  void println(std::string_view text = {});

  void print_color(std::string_view text, Color foreground, Color background = Color::Default,
                   bool bold = false);
  void println_color(std::string_view text, Color foreground, Color background = Color::Default,
                     bool bold = false);

  [[nodiscard]] std::string paint(std::string_view text, Color foreground,
                                  Color background = Color::Default, bool bold = false) const;

  void flush();

private:
  [[nodiscard]] static bool enable_virtual_terminal();
  [[nodiscard]] static int ansi_foreground(Color color);
  [[nodiscard]] static int ansi_background(Color color);

  bool ansi_enabled_{false};
};

} // namespace opentui
