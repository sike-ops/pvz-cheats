#pragma once

#include "win32.hpp"

#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <string_view>

namespace injector {
class PipeServer {
public:
  PipeServer() = default;
  PipeServer(PipeServer &&) = default;
  PipeServer &operator=(PipeServer &&) = default;
  PipeServer(const PipeServer &) = delete;
  PipeServer &operator=(const PipeServer &) = delete;

  void Open(std::wstring_view name);

  // Waits for the dll to connect. Returns false on timeout.
  bool Connect(std::chrono::milliseconds timeout);

  // Sends a single command as one pipe message. Throws on failure.
  void Send(std::uint32_t command);

  void Disconnect() noexcept;

  [[nodiscard]] bool IsOpen() const noexcept;

private:
  // Cancels a pending overlapped operation and reaps it so the pipe handle
  // stays usable.
  void CancelAndReap(OVERLAPPED &overlapped) noexcept;

  win32::UniqueHandle pipe_;
  win32::UniqueHandle event_;
};
} // namespace injector
