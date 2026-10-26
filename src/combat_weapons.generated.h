// Generated weapon impact metadata, never game assets.
#pragma once
#include <array>
namespace classic::impact {
struct Unarmed { static constexpr unsigned guestKind=0,nativeForm=80258;static constexpr int skill=-1; };
struct Blade { static constexpr unsigned guestKind=1,nativeForm=77495;static constexpr int skill=6; };
struct Axe { static constexpr unsigned guestKind=2,nativeForm=79760;static constexpr int skill=7; };
struct Blunt { static constexpr unsigned guestKind=3,nativeForm=80258;static constexpr int skill=6; };
struct Pierce { static constexpr unsigned guestKind=4,nativeForm=80254;static constexpr int skill=7; };
struct Arrow { static constexpr unsigned guestKind=5,nativeForm=80261;static constexpr int skill=8; };
struct Weapon {unsigned kind,form;int skill;};
inline constexpr std::array<Weapon,6> weapons{{{Unarmed::guestKind,Unarmed::nativeForm,Unarmed::skill},{Blade::guestKind,Blade::nativeForm,Blade::skill},{Axe::guestKind,Axe::nativeForm,Axe::skill},{Blunt::guestKind,Blunt::nativeForm,Blunt::skill},{Pierce::guestKind,Pierce::nativeForm,Pierce::skill},{Arrow::guestKind,Arrow::nativeForm,Arrow::skill}}};
}
