#pragma once

#include <cstdint>

namespace CemuExtendTextInputKeyPolicy
{
constexpr std::uint16_t RightShiftUsage = 0xE5;

// native IME が所有する通常の編集keyはguestへ二重配送しない。
// Right Shiftだけはguest側Menu toggleの契約があるためdown/upをmirrorする。
[[nodiscard]] constexpr bool ShouldMirror(bool nativeTextInputEvent,
                                          std::uint16_t usage,
                                          bool nativeSubmit) noexcept
{
    return !nativeTextInputEvent || nativeSubmit || usage == RightShiftUsage;
}
} // namespace CemuExtendTextInputKeyPolicy
