# open tui c++

`open tui c++` is a lightweight C++20 terminal UI library for building interactive CLI debuggers with minimal dependencies.

## Features

- Overridable banner and prompt through inheritance.
- Built-in `help`, `exit`, and `quit` commands.
- Simple command registration API with argument handlers.
- Interactive tab completion for commands and custom sub-arguments.
- Fine-grained colored output (ANSI, with Windows virtual terminal support).
- Signal-aware run loop for clean termination (`SIGINT`, `SIGTERM`, `SIGHUP` on POSIX).
- UDP send/receive utility for external agent communication.
- C++20, CMake, `.clang-format`, and `.clang-tidy` included.
- Cross-platform target: macOS, Linux (Ubuntu), and Windows.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Lint and Format

```bash
find include src examples -type f \( -name '*.hpp' -o -name '*.cpp' \) -print0 | \
  xargs -0 clang-format --dry-run --Werror
```

macOS (Homebrew LLVM) clang-tidy example:

```bash
SDKROOT=$(xcrun --show-sdk-path)
clang-tidy -p build src/*.cpp examples/debugger/main.cpp \
  --extra-arg=-isysroot --extra-arg=$SDKROOT --quiet
```

## Run Example Debugger

```bash
./build/open_tui_example
```

On Windows with Visual Studio generator:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\open_tui_example.exe
```

## Library Usage

1. Inherit from `opentui::TuiApplication`.
2. Override `banner()` and optionally `prompt()`, `on_start()`, `on_shutdown()`.
3. Implement `register_commands(opentui::CommandRegistry&)` and add commands.
4. Call `run()` from your `main()`.

Minimal example:

```cpp
class MyDebugger : public opentui::TuiApplication {
protected:
  std::string banner() const override { return "my debugger"; }

  void register_commands(opentui::CommandRegistry& registry) override {
    registry.add(opentui::Command{
      .name = "ping",
      .description = "prints pong",
      .handler = [](const opentui::Args&, opentui::CommandContext& ctx) {
        ctx.console.println("pong");
      },
      .completer = nullptr,
    });
  }
};
```

## Example Commands

- `help`: list commands
- `exit` / `quit`: terminate loop
- `status`: show mock debugger state
- `step [N]`: advance sample program counter
- `trace <on|off>`: toggle tracing
- `udp_send <host> <port> <message>`: send UDP packet
- `udp_wait <port> [timeout_ms]`: wait for one UDP packet
