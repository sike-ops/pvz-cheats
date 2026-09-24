#pragma once

#include "win32.hpp"

#include <Windows.h>

#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

namespace win32 {
inline std::wstring FormatWin32ErrorW(DWORD error) {
  if (error == ERROR_SUCCESS) {
    return L"no error";
  }

  LPWSTR buffer{nullptr};
  const DWORD length{FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPWSTR>(&buffer), 0, nullptr)};

  std::wstring message;

  if (length != 0 && buffer != nullptr) {
    message.assign(buffer, length);
    LocalFree(buffer);
  } else {
    message = L"unknown error";
  }

  while (!message.empty() &&
         (message.back() == L'\r' || message.back() == L'\n' ||
          message.back() == L' ')) {
    message.pop_back();
  }

  return message;
}

inline std::string FormatWin32Error(DWORD error) {
  return std::format("{} (Win32 error {}, 0x{:08X})",
                     WideToUtf8(FormatWin32ErrorW(error)), error, error);
}

[[noreturn]] inline void ThrowWin32Error(DWORD error,
                                         std::string_view context = {}) {
  const std::string message{FormatWin32Error(error)};

  if (context.empty()) {
    throw std::runtime_error(message);
  }

  throw std::runtime_error(std::format("{}: {}", context, message));
}

[[noreturn]] inline void ThrowLastError(std::string_view context = {}) {
  const DWORD error{GetLastError()};
  ThrowWin32Error(error, context);
}
} // namespace win32
