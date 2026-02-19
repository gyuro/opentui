#pragma once

#include <atomic>
#include <csignal>

namespace opentui {

class SignalManager {
public:
  SignalManager();
  ~SignalManager();

  SignalManager(const SignalManager&) = delete;
  SignalManager& operator=(const SignalManager&) = delete;

  [[nodiscard]] bool stop_requested() const noexcept;

  static void request_stop() noexcept;
  static void clear_stop() noexcept;

private:
  using SignalHandler = void (*)(int);

  static void on_signal(int signal) noexcept;

  SignalHandler previous_int_{SIG_DFL};
  SignalHandler previous_term_{SIG_DFL};
#if defined(SIGHUP)
  SignalHandler previous_hup_{SIG_DFL};
#endif

  static std::atomic_bool stop_requested_;
};

} // namespace opentui
