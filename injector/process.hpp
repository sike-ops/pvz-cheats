#pragma once

#include "win32.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace injector {
std::optional<std::uint32_t> FindProcessId(std::wstring_view executableName);
win32::UniqueHandle OpenProcess(std::uint32_t processId);
bool IsModuleLoaded(std::uint32_t processId, std::wstring_view moduleName);
} // namespace injector
