#include "process.hpp"

#include "error.hpp"

#include <Windows.h>
#include <tlhelp32.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace {
constexpr DWORD kInjectionAccess{
    PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
    PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE};
} // namespace

std::optional<std::uint32_t>
injector::FindProcessId(std::wstring_view executableName) {
  const win32::UniqueHandle snapshot{
      CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};

  if (!snapshot) {
    win32::ThrowLastError("couldn't enumerate the running processes");
  }

  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);

  if (!Process32FirstW(snapshot.Get(), &entry)) {
    if (GetLastError() == ERROR_NO_MORE_FILES) {
      return std::nullopt;
    }

    win32::ThrowLastError("couldn't enumerate the running processes");
  }

  const std::wstring name{executableName};

  do {
    if (entry.szExeFile[0] != L'\0' &&
        _wcsicmp(entry.szExeFile, name.c_str()) == 0) {
      return entry.th32ProcessID;
    }
  } while (Process32NextW(snapshot.Get(), &entry));

  return std::nullopt;
}

win32::UniqueHandle injector::OpenProcess(std::uint32_t processId) {
  return win32::UniqueHandle{::OpenProcess(kInjectionAccess, FALSE, processId)};
}

bool injector::IsModuleLoaded(std::uint32_t processId,
                              std::wstring_view moduleName) {
  const win32::UniqueHandle snapshot{CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId)};

  if (!snapshot) {
    return false;
  }

  MODULEENTRY32W entry{};
  entry.dwSize = sizeof(entry);

  if (!Module32FirstW(snapshot.Get(), &entry)) {
    return false;
  }

  const std::wstring name{moduleName};

  do {
    if (_wcsicmp(entry.szModule, name.c_str()) == 0) {
      return true;
    }
  } while (Module32NextW(snapshot.Get(), &entry));

  return false;
}
