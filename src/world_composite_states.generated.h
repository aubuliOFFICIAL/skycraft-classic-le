// Generated from sheets/world_composite_states.json. Do not edit.
#pragma once
#include <array>
namespace classic::composite {
struct SeparateAlpha { static constexpr D3DRENDERSTATETYPE state=D3DRS_SEPARATEALPHABLENDENABLE; static constexpr DWORD value=1; };
struct AlphaSource { static constexpr D3DRENDERSTATETYPE state=D3DRS_SRCBLENDALPHA; static constexpr DWORD value=D3DBLEND_ONE; };
struct AlphaDestination { static constexpr D3DRENDERSTATETYPE state=D3DRS_DESTBLENDALPHA; static constexpr DWORD value=D3DBLEND_INVSRCALPHA; };
struct AlphaAdd { static constexpr D3DRENDERSTATETYPE state=D3DRS_BLENDOPALPHA; static constexpr DWORD value=D3DBLENDOP_ADD; };
struct Setting { D3DRENDERSTATETYPE state; DWORD value; };
inline constexpr std::array<Setting,4> states = {{
{SeparateAlpha::state,SeparateAlpha::value},
{AlphaSource::state,AlphaSource::value},
{AlphaDestination::state,AlphaDestination::value},
{AlphaAdd::state,AlphaAdd::value},
}};
}