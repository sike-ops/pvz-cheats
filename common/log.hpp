#pragma once

#include <Windows.h>

#include <format>
#include <string>
#include <utility>

namespace logging {
template <class... Args>
void DebugMessage(std::format_string<Args...> format, Args &&...args) {
  std::string message{"PVZCHEAT|"};
  message += std::format(format, std::forward<Args>(args)...);
  message += '\n';

  OutputDebugStringA(message.c_str());
}
} // namespace logging
