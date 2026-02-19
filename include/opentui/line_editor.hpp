#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opentui {

class LineEditor {
public:
  using CompletionProvider = std::function<std::vector<std::string>(std::string_view)>;

  [[nodiscard]] std::optional<std::string> read_line(std::string_view prompt,
                                                     const CompletionProvider& completion_provider);

private:
  [[nodiscard]] static bool is_interactive();
};

} // namespace opentui
