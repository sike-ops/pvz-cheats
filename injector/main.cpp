#include <Windows.h>

#include "error.hpp"
#include "ui.hpp"
#include "win32.hpp"

#include <exception>
#include <string>

namespace {
constexpr wchar_t kSingleInstanceMutex[] = L"Local\\pvz-cheats-injector";
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  const win32::UniqueHandle mutex{
      CreateMutexW(nullptr, FALSE, kSingleInstanceMutex)};

  if (!mutex) {
    const std::wstring message{
        win32::Utf8ToWide(win32::FormatWin32Error(GetLastError()))};

    MessageBoxW(nullptr, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
    return 1;
  }

  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    MessageBoxW(nullptr, L"pvz-cheats-injector is already running.",
                L"Already running", MB_OK | MB_ICONINFORMATION);

    return 0;
  }

  try {
    ui::Init(instance);
    ui::MessageLoop();
  } catch (const std::exception &exception) {
    const std::wstring message{win32::Utf8ToWide(exception.what())};
    MessageBoxW(nullptr, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
    return 1;
  }

  return 0;
}
