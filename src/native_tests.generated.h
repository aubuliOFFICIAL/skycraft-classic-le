#pragma once
#include <array>
namespace classic::nativetest {
struct Pause {
static constexpr unsigned action=11;
static constexpr unsigned scanCode=1;
static constexpr unsigned holdMs=250;
};
struct Menu {
static constexpr unsigned action=12;
static constexpr unsigned scanCode=15;
static constexpr unsigned holdMs=250;
};
struct QuickSave {
static constexpr unsigned action=14;
static constexpr unsigned scanCode=63;
static constexpr unsigned holdMs=250;
};
struct QuickLoad {
static constexpr unsigned action=15;
static constexpr unsigned scanCode=67;
static constexpr unsigned holdMs=250;
};
struct Test {unsigned action,scan,hold;};
inline constexpr std::array<Test,4> tests={{
{Pause::action,Pause::scanCode,Pause::holdMs},
{Menu::action,Menu::scanCode,Menu::holdMs},
{QuickSave::action,QuickSave::scanCode,QuickSave::holdMs},
{QuickLoad::action,QuickLoad::scanCode,QuickLoad::holdMs},
}};
}
