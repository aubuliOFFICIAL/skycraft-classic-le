#pragma once
#include <array>
namespace classic::uitest {
struct Take {static constexpr unsigned action=24,scan=18,context=1;static constexpr const char* event="Accept";};
struct Close {static constexpr unsigned action=25,scan=15,context=1;static constexpr const char* event="Cancel";};
struct Test {unsigned action,scan,context;const char* event;};
inline constexpr std::array<Test,2> tests={{
{Take::action,Take::scan,Take::context,Take::event},
{Close::action,Close::scan,Close::context,Close::event},
}};
}
