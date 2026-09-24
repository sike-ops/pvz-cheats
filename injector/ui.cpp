#include "ui.hpp"

#include "cheat.hpp"
#include "error.hpp"
#include "protocol.hpp"
#include "win32.hpp"

#include <Windows.h>
#include <commdlg.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kWindowClassName[] = L"pvz-cheats-injector-window";
constexpr wchar_t kWindowTitle[] = L"PvZ cheat menu";

constexpr UINT_PTR kGamePollTimer{1};
constexpr UINT kGamePollIntervalMs{500};
constexpr UINT kHookFinishedMessage{WM_APP + 1};

constexpr int kClientWidth{300};
constexpr int kClientHeight{340};
constexpr int kMargin{10};
constexpr int kControlWidth{kClientWidth - 2 * kMargin};
constexpr int kButtonHeight{30};
constexpr int kRowHeight{40};

enum class CommandId : int {
  Hook = 1,
  Sun100,
  ZeroCost,
  ZeroWait,
  FastShooting,
  ImmortalPlants,
  Instakill,
};

struct CheatButton {
  CommandId id;
  protocol::Cheat cheat;
  std::wstring_view label;
};

constexpr std::array<CheatButton, 6> kCheatButtons{{
    {CommandId::Sun100, protocol::Cheat::Sun100, L"100 sun"},
    {CommandId::ZeroCost, protocol::Cheat::ZeroCost, L"zero cost"},
    {CommandId::ZeroWait, protocol::Cheat::ZeroWait, L"zero wait"},
    {CommandId::FastShooting, protocol::Cheat::FastShooting, L"fast shooting"},
    {CommandId::ImmortalPlants, protocol::Cheat::ImmortalPlants,
     L"immortal plants"},
    {CommandId::Instakill, protocol::Cheat::Instakill, L"insta kill"},
}};

struct AppState {
  HINSTANCE instance{};
  HWND window{};
  HWND status{};
  HWND hookButton{};
  std::array<HWND, kCheatButtons.size()> cheatButtons{};
  std::shared_ptr<cheat::Session> session{std::make_shared<cheat::Session>()};
  bool hookInProgress{false};
};

AppState *GetState(HWND window) noexcept {
  return reinterpret_cast<AppState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

void SetStatus(AppState *state, std::wstring_view text) {
  SetWindowTextW(state->status, std::wstring{text}.c_str());
}

void ShowError(HWND owner, std::string_view message) {
  const std::wstring text{win32::Utf8ToWide(message)};
  MessageBoxW(owner, text.c_str(), L"Error", MB_OK | MB_ICONERROR);
}

void UpdateControls(AppState *state) {
  const bool connected{state->session->IsConnected()};

  if (state->hookInProgress) {
    SetStatus(state, L"Injecting into PvZ...");
  } else if (connected) {
    SetStatus(state, L"Hooked into PvZ");
  } else {
    SetStatus(state, L"Not hooked");
  }

  EnableWindow(state->hookButton,
               !state->hookInProgress && !connected ? TRUE : FALSE);

  for (const HWND button : state->cheatButtons) {
    EnableWindow(button, connected ? TRUE : FALSE);
  }
}

const protocol::Cheat *FindCheat(CommandId id) noexcept {
  for (const CheatButton &button : kCheatButtons) {
    if (button.id == id) {
      return &button.cheat;
    }
  }

  return nullptr;
}

void CreateControls(AppState *state) {
  const HFONT font{reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT))};

  const auto createControl = [&](const wchar_t *className, const wchar_t *text,
                                 DWORD style, int x, int y, int width,
                                 int height, int id) {
    const HWND control{CreateWindowExW(
        0, className, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height,
        state->window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        state->instance, nullptr)};

    if (control == nullptr) {
      win32::ThrowLastError("couldn't create a window control");
    }

    SendMessageW(control, WM_SETFONT,
                 static_cast<WPARAM>(reinterpret_cast<INT_PTR>(font)), TRUE);

    return control;
  };

  state->hookButton = createControl(
      L"BUTTON", L"Hook PvZ", WS_TABSTOP | BS_DEFPUSHBUTTON, kMargin, kMargin,
      kControlWidth, kButtonHeight, static_cast<int>(CommandId::Hook));

  state->status = createControl(L"STATIC", L"Not hooked", SS_LEFT, kMargin, 45,
                                kControlWidth, 20, 0);

  createControl(L"BUTTON", L"cheats", WS_TABSTOP | BS_GROUPBOX, kMargin, 70,
                kControlWidth,
                20 + static_cast<int>(kCheatButtons.size()) * kRowHeight, 0);

  for (std::size_t i = 0; i < kCheatButtons.size(); i++) {
    state->cheatButtons[i] =
        createControl(L"BUTTON", std::wstring{kCheatButtons[i].label}.c_str(),
                      WS_TABSTOP | BS_PUSHBUTTON, kMargin + 10,
                      90 + static_cast<int>(i) * kRowHeight, kControlWidth - 20,
                      kButtonHeight, static_cast<int>(kCheatButtons[i].id));

    EnableWindow(state->cheatButtons[i], FALSE);
  }
}

std::wstring DefaultDllPath() {
  std::wstring buffer(MAX_PATH, L'\0');
  const DWORD length{GetModuleFileNameW(nullptr, buffer.data(),
                                        static_cast<DWORD>(buffer.size()))};
  buffer.resize(length);

  return (std::filesystem::path{buffer}.parent_path() / L"pvz-cheats.dll")
      .wstring();
}

bool PickDll(HWND owner, std::wstring &path) {
  std::vector<wchar_t> buffer(std::max<std::size_t>(path.size() + 1, MAX_PATH),
                              L'\0');
  std::copy(path.begin(), path.end(), buffer.begin());

  OPENFILENAMEW dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = owner;
  dialog.lpstrFilter = L"DLL files (*.dll)\0*.dll\0All files (*.*)\0*.*\0";
  dialog.lpstrFile = buffer.data();
  dialog.nMaxFile = static_cast<DWORD>(buffer.size());
  dialog.Flags =
      OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  dialog.lpstrDefExt = L"dll";

  if (GetOpenFileNameW(&dialog) == FALSE) {
    return false;
  }

  path = buffer.data();
  return true;
}

void StartHook(AppState *state, std::wstring dllPath) {
  state->hookInProgress = true;
  UpdateControls(state);

  const std::shared_ptr<cheat::Session> session{state->session};
  const HWND window{state->window};

  try {
    std::thread{[session, window, path = std::move(dllPath)] {
      std::wstring *error{nullptr};

      try {
        session->Hook(path);
      } catch (const std::exception &exception) {
        error = new std::wstring{win32::Utf8ToWide(exception.what())};
      } catch (...) {
        error = new std::wstring{L"unknown error"};
      }

      if (PostMessageW(window, kHookFinishedMessage, 0,
                       reinterpret_cast<LPARAM>(error)) == FALSE) {
        delete error;
      }
    }}.detach();
  } catch (const std::exception &exception) {
    state->hookInProgress = false;

    UpdateControls(state);
    ShowError(window, exception.what());
  }
}

void OnHookClicked(AppState *state) {
  std::wstring dllPath{DefaultDllPath()};

  if (!PickDll(state->window, dllPath)) {
    return;
  }

  StartHook(state, std::move(dllPath));
}

void OnCheatClicked(AppState *state, protocol::Cheat cheat) {
  try {
    state->session->Send(cheat);
  } catch (const std::exception &exception) {
    // Send() already dropped the broken connection.
    UpdateControls(state);
    ShowError(state->window, exception.what());
  }
}

void OnHookFinished(AppState *state, std::wstring *error) {
  state->hookInProgress = false;
  UpdateControls(state);

  if (error != nullptr) {
    MessageBoxW(state->window, error->c_str(), L"Injection failed",
                MB_OK | MB_ICONERROR);

    delete error;
  }
}

void OnPoll(AppState *state) {
  if (state->session->GameExited()) {
    state->session->Disconnect();
    UpdateControls(state);
  }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam,
                            LPARAM lParam) {
  switch (message) {
  case WM_NCCREATE: {
    auto *state{new (std::nothrow) AppState{}};
    if (state == nullptr) {
      return FALSE;
    }

    state->window = window;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

    return TRUE;
  }
  case WM_COMMAND: {
    AppState *state{GetState(window)};

    if (state == nullptr || HIWORD(wParam) != BN_CLICKED) {
      break;
    }

    const auto id{static_cast<CommandId>(LOWORD(wParam))};

    if (id == CommandId::Hook) {
      OnHookClicked(state);
      return 0;
    }

    if (const protocol::Cheat *cheat{FindCheat(id)}) {
      OnCheatClicked(state, *cheat);
      return 0;
    }
    break;
  }
  case WM_TIMER:
    if (wParam == kGamePollTimer) {
      if (AppState * state{GetState(window)}) {
        OnPoll(state);
      }

      return 0;
    }
    break;
  case kHookFinishedMessage:
    if (AppState * state{GetState(window)}) {
      OnHookFinished(state, reinterpret_cast<std::wstring *>(lParam));
    }

    return 0;
  case WM_CLOSE:
    DestroyWindow(window);
    return 0;
  case WM_DESTROY:
    KillTimer(window, kGamePollTimer);
    PostQuitMessage(0);
    return 0;
  case WM_NCDESTROY: {
    AppState *state{GetState(window)};
    SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    delete state;

    return DefWindowProcW(window, message, wParam, lParam);
  }
  }

  return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

void ui::Init(HINSTANCE instance) {
  const WNDCLASSEXW windowClass{
      .cbSize = sizeof(WNDCLASSEXW),
      .style = CS_HREDRAW | CS_VREDRAW,
      .lpfnWndProc = WindowProc,
      .hInstance = instance,
      .hCursor = LoadCursorW(nullptr, IDC_ARROW),
      .hbrBackground =
          reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1)),
      .lpszClassName = kWindowClassName,
  };

  if (RegisterClassExW(&windowClass) == 0 &&
      GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    win32::ThrowLastError("couldn't register the window class");
  }

  constexpr DWORD kWindowStyle{WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                               WS_MINIMIZEBOX};

  RECT windowRect{0, 0, kClientWidth, kClientHeight};
  AdjustWindowRect(&windowRect, kWindowStyle, FALSE);

  const HWND window{CreateWindowExW(
      0, kWindowClassName, kWindowTitle, kWindowStyle, CW_USEDEFAULT,
      CW_USEDEFAULT, windowRect.right - windowRect.left,
      windowRect.bottom - windowRect.top, nullptr, nullptr, instance, nullptr)};

  if (window == nullptr) {
    win32::ThrowLastError("couldn't create the main window");
  }

  AppState *state{GetState(window)};

  if (state == nullptr) {
    throw std::runtime_error("couldn't initialize the application state");
  }

  state->instance = instance;

  CreateControls(state);
  UpdateControls(state);

  SetTimer(window, kGamePollTimer, kGamePollIntervalMs, nullptr);
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);
}

void ui::MessageLoop() {
  MSG message{};

  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
}
