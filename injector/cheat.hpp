#pragma once

#include "channel.hpp"
#include "protocol.hpp"
#include "win32.hpp"

#include <mutex>
#include <string_view>

namespace cheat {
class Session {
public:
  Session() = default;
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;

  void Hook(std::wstring_view dllPath);
  void Send(protocol::Cheat cheat);
  void Disconnect();

  [[nodiscard]] bool IsConnected() const;
  [[nodiscard]] bool GameExited() const;

private:
  void Reset() noexcept;

  mutable std::mutex mutex_;
  win32::UniqueHandle process_;
  injector::PipeServer pipe_;
  bool connected_{false};
};
} // namespace cheat
