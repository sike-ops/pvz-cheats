#pragma once

#include <cstdint>
#include <string_view>

namespace protocol {
inline constexpr std::wstring_view kPipeName{
    L"\\\\.\\pipe\\9e9bac38-db8d-4960-abb9-8e513c28b56c"};

inline constexpr std::wstring_view kGameProcessName{L"popcapgame1.exe"};

enum class Cheat : std::uint32_t {
  Instakill = 0x41894ca7,
  ImmortalPlants = 0x75c3af74,
  FastShooting = 0xbb9b944a,
  ZeroWait = 0x371fff6e,
  ZeroCost = 0x445e571a,
  Sun100 = 0xaef946d3,
};
} // namespace protocol
