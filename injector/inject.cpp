#include "inject.hpp"

#include "error.hpp"
#include "win32.hpp"

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
constexpr DWORD kLoadTimeoutMs{10'000};

class RemoteAllocation {
public:
  RemoteAllocation(HANDLE process, void *address) noexcept
      : process_{process}, address_{address} {}

  RemoteAllocation(const RemoteAllocation &) = delete;
  RemoteAllocation &operator=(const RemoteAllocation &) = delete;

  ~RemoteAllocation() { Reset(); }

  void Reset() noexcept {
    if (address_ != nullptr) {
      VirtualFreeEx(process_, address_, 0, MEM_RELEASE);
      address_ = nullptr;
    }
  }

  void Release() noexcept { address_ = nullptr; }

private:
  HANDLE process_{};
  void *address_{};
};

} // namespace

void injector::InjectDll(HANDLE process, std::wstring_view dllPath) {
  const HMODULE kernel32{GetModuleHandleW(L"kernel32.dll")};
  const auto loadLibrary{reinterpret_cast<LPTHREAD_START_ROUTINE>(
      GetProcAddress(kernel32, "LoadLibraryW"))};

  if (loadLibrary == nullptr) {
    win32::ThrowLastError("couldn't resolve LoadLibraryW");
  }

  const std::wstring path{dllPath};
  const std::size_t pathBytes{(path.size() + 1) * sizeof(wchar_t)};

  void *remotePath{VirtualAllocEx(process, nullptr, pathBytes,
                                  MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)};

  if (remotePath == nullptr) {
    win32::ThrowLastError("couldn't allocate memory in the game process");
  }

  RemoteAllocation allocation{process, remotePath};

  if (!WriteProcessMemory(process, remotePath, path.c_str(), pathBytes,
                          nullptr)) {
    win32::ThrowLastError("couldn't write the dll path into the game process");
  }

  const win32::UniqueHandle thread{CreateRemoteThread(
      process, nullptr, 0, loadLibrary, remotePath, 0, nullptr)};

  if (!thread) {
    win32::ThrowLastError("couldn't start the remote thread");
  }

  if (WaitForSingleObject(thread.Get(), kLoadTimeoutMs) != WAIT_OBJECT_0) {
    allocation.Release();
    throw std::runtime_error(
        "timed out waiting for the dll to load in the game process");
  }

  DWORD exitCode{};

  if (!GetExitCodeThread(thread.Get(), &exitCode)) {
    win32::ThrowLastError("couldn't read the remote thread result");
  }

  if (exitCode == 0) {
    throw std::runtime_error(
        "the game failed to load the dll (LoadLibraryW returned null)");
  }
}
