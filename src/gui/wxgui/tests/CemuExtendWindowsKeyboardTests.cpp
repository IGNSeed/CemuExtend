#include "gui/wxgui/CemuExtendWindowsKeyboard.h"

#include <cstdlib>

namespace
{
void Check(bool condition)
{
	if (!condition)
		std::abort();
}
}

int main()
{
	using namespace CemuExtendWindowsKeyboard;

	auto event = Decode(0x10, 0x36, 0);
	Check(event.usage == 0xe5 && event.pressed);
	event = Decode(0x10, 0x36, RawKeyBreak);
	Check(event.usage == 0xe5 && !event.pressed);
	Check(Decode(0x10, 0x2a, 0).usage == 0xe1);
	Check(Decode(0x11, 0x1d, RawKeyE0).usage == 0xe4);
	Check(Decode(0x0d, 0x1c, 0).usage == 0x28);
	Check(Decode(0x0d, 0x1c, RawKeyE0).usage == 0x58);
	Check(Decode('A', 0x1e, 0).usage == 0x04);
	Check(Decode(0x61, 0x4f, 0).usage == 0x59);
	Check(Decode(0x69, 0x49, 0).usage == 0x61);
	Check(Decode(0x7b, 0x58, 0).usage == 0x45);
	Check(Decode(0x7c, 0, 0).usage == 0x68);
	Check(Decode(0x00ff, 0, 0).usage == 0);

	ModifierState modifiers;
	modifiers.Apply(0xe1, true);
	modifiers.Apply(0xe5, true);
	Check(modifiers.GenericMask() == 2);
	modifiers.Apply(0xe1, false);
	Check(modifiers.GenericMask() == 2);
	modifiers.Apply(0xe5, false);
	Check(modifiers.GenericMask() == 0);
	modifiers.Apply(0xe4, true);
	modifiers.Apply(0xe6, true);
	Check(modifiers.GenericMask() == 5);
	modifiers.Reset();
	Check(modifiers.GenericMask() == 0);
	return 0;
}
