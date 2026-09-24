#pragma once

#include <Windows.h>

#include <string>
#include <string_view>
#include <utility>

namespace win32 {
class UniqueHandle {
public:
  UniqueHandle() = default;

  explicit UniqueHandle(HANDLE handle) noexcept : handle_{handle} {}

  UniqueHandle(UniqueHandle &&other) noexcept
      : handle_{std::exchange(other.handle_, nullptr)} {}

  UniqueHandle &operator=(UniqueHandle &&other) noexcept {
    if (this != &other) {
      Reset(std::exchange(other.handle_, nullptr));
    }

    return *this;
  }

  UniqueHandle(const UniqueHandle &) = delete;
  UniqueHandle &operator=(const UniqueHandle &) = delete;

  ~UniqueHandle() { Reset(); }

  [[nodiscard]] HANDLE Get() const noexcept { return handle_; }

  [[nodiscard]] explicit operator bool() const noexcept {
    return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
  }

  [[nodiscard]] HANDLE Release() noexcept {
    return std::exchange(handle_, nullptr);
  }

  void Reset(HANDLE handle = nullptr) noexcept {
    if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
    }

    handle_ = handle;
  }

private:
  HANDLE handle_{nullptr};
};

inline std::string WideToUtf8(std::wstring_view text) {
  if (text.empty()) {
    return {};
  }

  const int size{WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                     static_cast<int>(text.size()), nullptr, 0,
                                     nullptr, nullptr)};

  if (size <= 0) {
    return {};
  }

  std::string result(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      result.data(), size, nullptr, nullptr);

  return result;
}

inline std::wstring Utf8ToWide(std::string_view text) {
  if (text.empty()) {
    return {};
  }

  const int size{MultiByteToWideChar(
      CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0)};

  if (size <= 0) {
    return {};
  }

  std::wstring result(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      result.data(), size);
  return result;
}
} // namespace win32
