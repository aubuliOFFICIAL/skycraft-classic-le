#pragma once
#include <array>
namespace classic::nativeinput {
struct Activate {static constexpr const char* event="Activate";static constexpr unsigned code=255;};
struct Favorites {static constexpr const char* event="Favorites";static constexpr unsigned code=255;};
struct Wait {static constexpr const char* event="Wait";static constexpr unsigned code=35;};
struct Binding {const char* event;unsigned code;};
inline constexpr std::array<Binding,3> bindings={{
{Activate::event,Activate::code},
{Favorites::event,Favorites::code},
{Wait::event,Wait::code},
}};
}
