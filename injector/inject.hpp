#pragma once

#include <Windows.h>

#include <string_view>

namespace injector {
void InjectDll(HANDLE process, std::wstring_view dllPath);
} // namespace injector
