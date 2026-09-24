#include <Windows.h>

#include "error.hpp"
#include "log.hpp"
#include "protocol.hpp"
#include "win32.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>

namespace {

namespace hack {

HMODULE hGame{nullptr};
constexpr std::size_t kPlantCount{52};

template <class T> T Read(std::uintptr_t offset) {
  T value{};

  std::memcpy(&value,
              reinterpret_cast<const void *>(
                  reinterpret_cast<std::uintptr_t>(hGame) + offset),
              sizeof(value));

  return value;
}

template <class T> void Write(std::uintptr_t offset, T value) {
  const std::uintptr_t address{reinterpret_cast<std::uintptr_t>(hGame) +
                               offset};

  std::memcpy(reinterpret_cast<void *>(address), &value, sizeof(value));

  FlushInstructionCache(GetCurrentProcess(),
                        reinterpret_cast<const void *>(address), sizeof(value));
}

template <std::size_t N>
void WriteBytes(std::uintptr_t offset,
                const std::array<std::uint8_t, N> &bytes) {
  const std::uintptr_t address{reinterpret_cast<std::uintptr_t>(hGame) +
                               offset};

  std::memcpy(reinterpret_cast<void *>(address), bytes.data(), bytes.size());

  FlushInstructionCache(GetCurrentProcess(),
                        reinterpret_cast<const void *>(address), bytes.size());
}

// rewrites the displacement of an existing relative call (0xE8).
void WriteCall(std::uintptr_t offset, const void *destination) {
  const std::uintptr_t callSite{reinterpret_cast<std::uintptr_t>(hGame) +
                                offset};
  const auto relative{static_cast<std::int32_t>(
      reinterpret_cast<std::uintptr_t>(destination) - (callSite + 5))};

  Write<std::int32_t>(offset + 1, relative);
}

void Instakill() {
  logging::DebugMessage("instakill activated");

  // regular zombies: inverse 'jg' to 'jle'
  Write<std::uint8_t>(0x14626C, 0x7A);

  // helmets: change 'sub ecx, eax' to 'xor ecx, ecx'
  Write<std::uint16_t>(0x145B14, 0xC931);

  // newspapers: inverse 'jg' to 'jl'
  Write<std::uint8_t>(0x145741, 0x7C);

  // doors: change 'sub dword [esi+0xdc], eax' to 'and dword [esi+0xdc], ebx'
  Write<std::uint16_t>(0x145771, 0x9E21);
}

void ImmortalPlants() {
  logging::DebugMessage("immortal plants activated");

  // "add dword [esi+0x40], 0xfffffffc" to "add dword [esi+0x40], 0x00"
  Write<std::uint8_t>(0x1447A0, 0x00);
}

void FastShooting() {
  logging::DebugMessage("fast shooting activated");

  // plants global variable, shooting timer
  constexpr std::uintptr_t kPlantTimers{0x326988 + 12};

  for (std::size_t i = 0; i < kPlantCount; i++) {
    Write<std::uint8_t>(kPlantTimers + i * 0x24, 0x32);
  }
}

void ZeroWait() {
  logging::DebugMessage("zero wait activated");

  // plant timer lookup: return 0 instead of
  // "mov eax, dword ptr [4*edx + 0x72698c]"
  constexpr std::array<std::uint8_t, 7> kZeroPlantTimer{0x31, 0xC0, 0x90, 0x90,
                                                        0x90, 0x90, 0x90};

  WriteBytes(0x97616, kZeroPlantTimer);
}

void ZeroCost() {
  logging::DebugMessage("zero cost activated");

  // change "setle al" to "or cl, 1" while keeping the original
  // "ret 4" opcode (0xC2) that follows it in place
  std::uint32_t instruction{Read<std::uint32_t>(0x1F67F)};
  instruction &= 0xFFu << 24;
  instruction |= 0x01C980;
  Write<std::uint32_t>(0x1F67F, instruction);

  // nop'd "jg ..; sub esi, ebx"
  Write<std::uint32_t>(0x1F632, 0x90909090);
}

std::int32_t *sunPointer{nullptr};

void Sun100() {
  logging::DebugMessage("adding 100 sun");

  if (sunPointer == nullptr) {
    return;
  }

  *reinterpret_cast<std::int32_t *>(
      reinterpret_cast<std::uintptr_t>(sunPointer) + 0x5578) += 100;
}

void HookApp() {
  void *cave{VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE,
                          PAGE_EXECUTE_READWRITE)};

  if (cave == nullptr) {
    logging::DebugMessage("failed to allocate memory for the code cave: {}",
                          win32::FormatWin32Error(GetLastError()));
    return;
  }

  auto *bytes{static_cast<std::uint8_t *>(cave)};

  // mov [sunPointer], edx
  bytes[0] = 0x89;
  bytes[1] = 0x15;
  const auto sunPointerSlot{reinterpret_cast<std::uintptr_t>(&sunPointer)};
  std::memcpy(bytes + 2, &sunPointerSlot, sizeof(sunPointerSlot));

  // jmp popcapgame1.exe+0x1f670
  bytes[6] = 0xE9;
  const auto relative{static_cast<std::int32_t>(
      reinterpret_cast<std::uintptr_t>(hGame) + 0x1F670 -
      (reinterpret_cast<std::uintptr_t>(cave) + 11))};
  std::memcpy(bytes + 7, &relative, sizeof(relative));

  // int3
  constexpr std::uint32_t kInt3{0xCCCCCCCC};
  std::memcpy(bytes + 13, &kInt3, sizeof(kInt3));

  FlushInstructionCache(GetCurrentProcess(), cave, 0x10);

  WriteCall(0x12DC0, cave);
  WriteCall(0x96B14, cave);
  WriteCall(0x96C00, cave);
  WriteCall(0x96EB6, cave);
}

const std::map<protocol::Cheat, void (*)()> kCheatTable{
    {protocol::Cheat::Sun100, &Sun100},
    {protocol::Cheat::ZeroCost, &ZeroCost},
    {protocol::Cheat::ZeroWait, &ZeroWait},
    {protocol::Cheat::FastShooting, &FastShooting},
    {protocol::Cheat::ImmortalPlants, &ImmortalPlants},
    {protocol::Cheat::Instakill, &Instakill},
};

} // namespace hack

void UnprotectTextSection(HMODULE module) {
  const std::uintptr_t base{reinterpret_cast<std::uintptr_t>(module)};
  const auto *dosHeader{reinterpret_cast<const IMAGE_DOS_HEADER *>(base)};
  const auto *ntHeaders{
      reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dosHeader->e_lfanew)};

  if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
    return;
  }

  const IMAGE_SECTION_HEADER *section{IMAGE_FIRST_SECTION(ntHeaders)};

  for (std::size_t i = 0; i < ntHeaders->FileHeader.NumberOfSections;
       i++, section++) {
    if (std::strncmp(reinterpret_cast<const char *>(section->Name), ".text",
                     5) == 0) {
      DWORD oldProtect{};
      VirtualProtect(reinterpret_cast<void *>(base + section->VirtualAddress),
                     section->Misc.VirtualSize, PAGE_EXECUTE_READWRITE,
                     &oldProtect);
    }
  }
}

win32::UniqueHandle ConnectToPipe() {
  return win32::UniqueHandle{CreateFileW(protocol::kPipeName.data(),
                                         GENERIC_READ | GENERIC_WRITE, 0,
                                         nullptr, OPEN_EXISTING, 0, nullptr)};
}

void HackThread() {
  const HMODULE gameModule{GetModuleHandleW(protocol::kGameProcessName.data())};
  if (gameModule == nullptr) {
    logging::DebugMessage("couldn't find the game module");
    return;
  }

  hack::hGame = gameModule;
  UnprotectTextSection(gameModule);
  hack::HookApp();

  constexpr DWORD kReconnectDelayMs{1000};

  // reconnect forever: the injector may be closed and reopened while the game
  // keeps running, so the dll must re-attach on its own.
  for (;;) {
    win32::UniqueHandle pipe{ConnectToPipe()};

    if (!pipe) {
      Sleep(kReconnectDelayMs);
      continue;
    }

    DWORD readMode{PIPE_READMODE_MESSAGE};
    SetNamedPipeHandleState(pipe.Get(), &readMode, nullptr, nullptr);
    logging::DebugMessage("connected to the injector");

    DWORD command{};
    DWORD bytesRead{};

    while (
        ReadFile(pipe.Get(), &command, sizeof(command), &bytesRead, nullptr)) {
      if (bytesRead != sizeof(command)) {
        continue;
      }

      const auto cheat{
          hack::kCheatTable.find(static_cast<protocol::Cheat>(command))};

      if (cheat != hack::kCheatTable.end()) {
        cheat->second();
      } else {
        logging::DebugMessage("unknown cheat command 0x{:08X}", command);
      }
    }

    logging::DebugMessage("injector disconnected; waiting for it to reconnect");
    Sleep(kReconnectDelayMs);
  }
}
} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(instance);

    if (CreateThread(nullptr, 0,
                     reinterpret_cast<LPTHREAD_START_ROUTINE>(&HackThread),
                     nullptr, 0, nullptr) == nullptr) {
      logging::DebugMessage("failed to create the hack thread");

      return FALSE;
    }
  }

  return TRUE;
}
