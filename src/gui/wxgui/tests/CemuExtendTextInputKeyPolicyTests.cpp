#include "gui/wxgui/CemuExtendTextInputKeyPolicy.h"

#include <cstdlib>
#include <iostream>

namespace
{
void Check(bool condition, const char* expression, int line)
{
    if (!condition)
    {
        std::cerr << "CHECK failed at line " << line << ": " << expression << '\n';
        std::abort();
    }
}
#define CHECK(condition) Check((condition), #condition, __LINE__)
} // namespace

int main()
{
    using CemuExtendTextInputKeyPolicy::ShouldMirror;

    CHECK(ShouldMirror(false, 0x04, false));
    CHECK(!ShouldMirror(true, 0x04, false));
    CHECK(!ShouldMirror(true, 0xE1, false));
    CHECK(ShouldMirror(true, 0xE5, false));
    CHECK(ShouldMirror(true, 0x28, true));
    CHECK(ShouldMirror(true, 0x58, true));
    return 0;
}
