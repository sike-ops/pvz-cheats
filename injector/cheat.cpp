#include "cheat.hpp"

#include "error.hpp"
#include "inject.hpp"
#include "log.hpp"
#include "process.hpp"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
constexpr std::chrono::milliseconds kConnectTimeout{5'000};
} // namespace

void cheat::Session::Hook(std::wstring_view dllPath) {
  const std::optional<std::uint32_t> processId{
      injector::FindProcessId(protocol::kGameProcessName)};

  if (!processId) {
    throw std::runtime_error("PvZ isn't running; start popcapgame1.exe first");
  }

  win32::UniqueHandle process{injector::OpenProcess(*processId)};

  if (!process) {
    win32::ThrowLastError("couldn't open the game process");
  }

  injector::PipeServer pipe;
  pipe.Open(protocol::kPipeName);

  const std::wstring moduleName{
      std::filesystem::path{dllPath}.filename().wstring()};

  if (injector::IsModuleLoaded(*processId, moduleName)) {
    logging::DebugMessage("{} is already loaded; waiting for it to reconnect",
                          std::filesystem::path{dllPath}.filename().string());
  } else {
    logging::DebugMessage("injecting {}", win32::WideToUtf8(dllPath));
    injector::InjectDll(process.Get(), dllPath);
  }

  if (!pipe.Connect(kConnectTimeout)) {
    throw std::runtime_error(
        "the dll didn't connect within 5 seconds; is it a valid build?");
  }

  std::lock_guard lock{mutex_};
  pipe_ = std::move(pipe);
  process_ = std::move(process);
  connected_ = true;

  logging::DebugMessage("hooked into PvZ (pid {})", *processId);
}

void cheat::Session::Send(protocol::Cheat cheat) {
  std::lock_guard lock{mutex_};

  if (!connected_) {
    throw std::runtime_error("not hooked into PvZ");
  }

  try {
    pipe_.Send(static_cast<std::uint32_t>(cheat));
  } catch (...) {
    Reset();
    throw;
  }
}

void cheat::Session::Disconnect() {
  std::lock_guard lock{mutex_};
  Reset();
}

bool cheat::Session::IsConnected() const {
  std::lock_guard lock{mutex_};
  return connected_;
}

bool cheat::Session::GameExited() const {
  std::lock_guard lock{mutex_};

  if (!connected_ || !process_) {
    return false;
  }

  return WaitForSingleObject(process_.Get(), 0) == WAIT_OBJECT_0;
}

void cheat::Session::Reset() noexcept {
  connected_ = false;
  pipe_ = injector::PipeServer{};
  process_.Reset();
}
