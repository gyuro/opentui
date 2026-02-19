#include "opentui/signal_manager.hpp"

namespace opentui {

std::atomic_bool SignalManager::stop_requested_{false};

SignalManager::SignalManager() {
  clear_stop();

  previous_int_ = std::signal(SIGINT, &SignalManager::on_signal);
  previous_term_ = std::signal(SIGTERM, &SignalManager::on_signal);
#if defined(SIGHUP)
  previous_hup_ = std::signal(SIGHUP, &SignalManager::on_signal);
#endif
}

SignalManager::~SignalManager() {
  std::signal(SIGINT, previous_int_);
  std::signal(SIGTERM, previous_term_);
#if defined(SIGHUP)
  std::signal(SIGHUP, previous_hup_);
#endif
}

bool SignalManager::stop_requested() const noexcept {
  return stop_requested_.load();
}

void SignalManager::request_stop() noexcept {
  stop_requested_.store(true);
}

void SignalManager::clear_stop() noexcept {
  stop_requested_.store(false);
}

void SignalManager::on_signal(const int signal) noexcept {
  static_cast<void>(signal);
  request_stop();
}

} // namespace opentui
