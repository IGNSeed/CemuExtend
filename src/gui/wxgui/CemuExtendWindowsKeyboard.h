#pragma once

#include <cstdint>

namespace CemuExtendWindowsKeyboard
{
constexpr std::uint16_t RawKeyBreak = 0x0001;
constexpr std::uint16_t RawKeyE0 = 0x0002;

struct KeyEvent
{
	std::uint16_t usage{};
	bool pressed{};
};

// Windows Raw Inputの仮想キーと拡張フラグをUSB HID Keyboard Usageへ変換する。
// wxWidgetsの子ウィンドウへキーイベントが届かない場合も同じ物理キーを識別できる。
[[nodiscard]] constexpr KeyEvent Decode(std::uint16_t virtualKey,
	std::uint16_t makeCode, std::uint16_t flags) noexcept
{
	const bool extended = (flags & RawKeyE0) != 0;
	const bool pressed = (flags & RawKeyBreak) == 0;

	if (virtualKey == 0x00ff)
		return {};

	if (virtualKey >= 'A' && virtualKey <= 'Z')
		return {static_cast<std::uint16_t>(0x04 + virtualKey - 'A'), pressed};
	if (virtualKey >= '1' && virtualKey <= '9')
		return {static_cast<std::uint16_t>(0x1e + virtualKey - '1'), pressed};
	if (virtualKey == '0')
		return {0x27, pressed};
	if (virtualKey >= 0x70 && virtualKey <= 0x7b) // VK_F1..VK_F12
		return {static_cast<std::uint16_t>(0x3a + virtualKey - 0x70), pressed};
	if (virtualKey >= 0x7c && virtualKey <= 0x87) // VK_F13..VK_F24
		return {static_cast<std::uint16_t>(0x68 + virtualKey - 0x7c), pressed};
	if (virtualKey == 0x60) // VK_NUMPAD0
		return {0x62, pressed};
	if (virtualKey >= 0x61 && virtualKey <= 0x69) // VK_NUMPAD1..VK_NUMPAD9
		return {static_cast<std::uint16_t>(0x59 + virtualKey - 0x61), pressed};

	switch (virtualKey)
	{
	case 0x08: return {0x2a, pressed}; // VK_BACK
	case 0x09: return {0x2b, pressed}; // VK_TAB
	case 0x0d: return {static_cast<std::uint16_t>(extended ? 0x58 : 0x28), pressed};
	case 0x10: return {static_cast<std::uint16_t>(makeCode == 0x36 ? 0xe5 : 0xe1), pressed};
	case 0x11: return {static_cast<std::uint16_t>(extended ? 0xe4 : 0xe0), pressed};
	case 0x12: return {static_cast<std::uint16_t>(extended ? 0xe6 : 0xe2), pressed};
	case 0x13: return {0x48, pressed}; // VK_PAUSE
	case 0x14: return {0x39, pressed}; // VK_CAPITAL
	case 0x1b: return {0x29, pressed}; // VK_ESCAPE
	case 0x20: return {0x2c, pressed}; // VK_SPACE
	case 0x21: return {static_cast<std::uint16_t>(extended ? 0x4b : 0x61), pressed};
	case 0x22: return {static_cast<std::uint16_t>(extended ? 0x4e : 0x5b), pressed};
	case 0x23: return {static_cast<std::uint16_t>(extended ? 0x4d : 0x59), pressed};
	case 0x24: return {static_cast<std::uint16_t>(extended ? 0x4a : 0x5f), pressed};
	case 0x25: return {static_cast<std::uint16_t>(extended ? 0x50 : 0x5c), pressed};
	case 0x26: return {static_cast<std::uint16_t>(extended ? 0x52 : 0x60), pressed};
	case 0x27: return {static_cast<std::uint16_t>(extended ? 0x4f : 0x5e), pressed};
	case 0x28: return {static_cast<std::uint16_t>(extended ? 0x51 : 0x5a), pressed};
	case 0x2c: return {0x46, pressed}; // VK_SNAPSHOT
	case 0x2d: return {static_cast<std::uint16_t>(extended ? 0x49 : 0x62), pressed};
	case 0x2e: return {static_cast<std::uint16_t>(extended ? 0x4c : 0x63), pressed};
	case 0x5b: return {0xe3, pressed}; // VK_LWIN
	case 0x5c: return {0xe7, pressed}; // VK_RWIN
	case 0x5d: return {0x65, pressed}; // VK_APPS
	case 0x6a: return {0x55, pressed}; // VK_MULTIPLY
	case 0x6b: return {0x57, pressed}; // VK_ADD
	case 0x6c: return {0x85, pressed}; // VK_SEPARATOR
	case 0x6d: return {0x56, pressed}; // VK_SUBTRACT
	case 0x6e: return {0x63, pressed}; // VK_DECIMAL
	case 0x6f: return {0x54, pressed}; // VK_DIVIDE
	case 0x90: return {0x53, pressed}; // VK_NUMLOCK
	case 0x91: return {0x47, pressed}; // VK_SCROLL
	case 0xa0: return {0xe1, pressed}; // VK_LSHIFT
	case 0xa1: return {0xe5, pressed}; // VK_RSHIFT
	case 0xa2: return {0xe0, pressed}; // VK_LCONTROL
	case 0xa3: return {0xe4, pressed}; // VK_RCONTROL
	case 0xa4: return {0xe2, pressed}; // VK_LMENU
	case 0xa5: return {0xe6, pressed}; // VK_RMENU
	case 0xba: return {0x33, pressed}; // VK_OEM_1
	case 0xbb: return {0x2e, pressed}; // VK_OEM_PLUS
	case 0xbc: return {0x36, pressed}; // VK_OEM_COMMA
	case 0xbd: return {0x2d, pressed}; // VK_OEM_MINUS
	case 0xbe: return {0x37, pressed}; // VK_OEM_PERIOD
	case 0xbf: return {0x38, pressed}; // VK_OEM_2
	case 0xc0: return {0x35, pressed}; // VK_OEM_3
	case 0xdb: return {0x2f, pressed}; // VK_OEM_4
	case 0xdc: return {0x31, pressed}; // VK_OEM_5
	case 0xdd: return {0x30, pressed}; // VK_OEM_6
	case 0xde: return {0x34, pressed}; // VK_OEM_7
	case 0xe2: return {0x64, pressed}; // VK_OEM_102
	default: return {};
	}
}

class ModifierState final
{
public:
	void Apply(std::uint16_t usage, bool pressed) noexcept
	{
		if (usage < 0xe0 || usage > 0xe7)
			return;
		const auto bit = static_cast<std::uint8_t>(1U << (usage - 0xe0));
		if (pressed)
			m_pressed |= bit;
		else
			m_pressed &= static_cast<std::uint8_t>(~bit);
	}

	[[nodiscard]] std::uint8_t GenericMask() const noexcept
	{
		return ((m_pressed & 0x11U) != 0 ? 1U : 0U) |
			((m_pressed & 0x22U) != 0 ? 2U : 0U) |
			((m_pressed & 0x44U) != 0 ? 4U : 0U) |
			((m_pressed & 0x88U) != 0 ? 8U : 0U);
	}

	void Reset() noexcept { m_pressed = 0; }

private:
	std::uint8_t m_pressed{};
};
} // namespace CemuExtendWindowsKeyboard
