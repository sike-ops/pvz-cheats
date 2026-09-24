#include "channel.hpp"

#include "error.hpp"

#include <Windows.h>

#include <chrono>
#include <stdexcept>
#include <string>

namespace {
constexpr DWORD kBufferSize{1024};
constexpr std::chrono::milliseconds kCommandTimeout{2'000};

DWORD ToMilliseconds(std::chrono::milliseconds timeout) {
  return static_cast<DWORD>(timeout.count());
}
} // namespace

void injector::PipeServer::Open(std::wstring_view name) {
  pipe_.Reset(CreateNamedPipeW(
      std::wstring{name}.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
          PIPE_REJECT_REMOTE_CLIENTS,
      PIPE_UNLIMITED_INSTANCES, kBufferSize, kBufferSize, 0, nullptr));

  if (!pipe_) {
    win32::ThrowLastError("couldn't create the command pipe");
  }

  event_.Reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));

  if (!event_) {
    win32::ThrowLastError("couldn't create the pipe event");
  }
}

bool injector::PipeServer::Connect(std::chrono::milliseconds timeout) {
  OVERLAPPED overlapped{};
  overlapped.hEvent = event_.Get();
  ResetEvent(event_.Get());

  if (ConnectNamedPipe(pipe_.Get(), &overlapped)) {
    return true;
  }

  const DWORD error{GetLastError()};

  if (error == ERROR_PIPE_CONNECTED) {
    return true;
  }

  if (error != ERROR_IO_PENDING) {
    win32::ThrowWin32Error(error, "couldn't wait for the game to connect");
  }

  if (WaitForSingleObject(event_.Get(), ToMilliseconds(timeout)) !=
      WAIT_OBJECT_0) {
    CancelAndReap(overlapped);
    return false;
  }

  DWORD transferred{};

  if (!GetOverlappedResult(pipe_.Get(), &overlapped, &transferred, FALSE)) {
    win32::ThrowLastError("couldn't wait for the game to connect");
  }

  return true;
}

void injector::PipeServer::Send(std::uint32_t command) {
  OVERLAPPED overlapped{};
  overlapped.hEvent = event_.Get();
  ResetEvent(event_.Get());

  DWORD written{};

  if (WriteFile(pipe_.Get(), &command, sizeof(command), &written,
                &overlapped)) {
    return;
  }

  const DWORD error{GetLastError()};

  if (error != ERROR_IO_PENDING) {
    win32::ThrowWin32Error(error, "couldn't send the cheat command");
  }

  if (WaitForSingleObject(event_.Get(), ToMilliseconds(kCommandTimeout)) !=
      WAIT_OBJECT_0) {
    CancelAndReap(overlapped);
    throw std::runtime_error("timed out sending the cheat command");
  }

  DWORD transferred{};

  if (!GetOverlappedResult(pipe_.Get(), &overlapped, &transferred, FALSE)) {
    win32::ThrowLastError("couldn't send the cheat command");
  }
}

void injector::PipeServer::Disconnect() noexcept {
  if (pipe_) {
    DisconnectNamedPipe(pipe_.Get());
  }
}

bool injector::PipeServer::IsOpen() const noexcept {
  return static_cast<bool>(pipe_);
}

void injector::PipeServer::CancelAndReap(OVERLAPPED &overlapped) noexcept {
  CancelIoEx(pipe_.Get(), &overlapped);
  WaitForSingleObject(event_.Get(), INFINITE);

  DWORD transferred{};
  GetOverlappedResult(pipe_.Get(), &overlapped, &transferred, FALSE);
}
