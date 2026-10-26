// Generated from sheets/world_render_states.json. Do not edit.
#pragma once
#include <array>
namespace classic::world {
struct DepthTest { static constexpr D3DRENDERSTATETYPE state=D3DRS_ZENABLE; static constexpr DWORD value=D3DZB_TRUE; };
struct PreserveNativeDepth { static constexpr D3DRENDERSTATETYPE state=D3DRS_ZWRITEENABLE; static constexpr DWORD value=0; };
struct DepthCompare { static constexpr D3DRENDERSTATETYPE state=D3DRS_ZFUNC; static constexpr DWORD value=D3DCMP_LESSEQUAL; };
struct NoFixedLights { static constexpr D3DRENDERSTATETYPE state=D3DRS_LIGHTING; static constexpr DWORD value=0; };
struct NoFog { static constexpr D3DRENDERSTATETYPE state=D3DRS_FOGENABLE; static constexpr DWORD value=0; };
struct NoStencil { static constexpr D3DRENDERSTATETYPE state=D3DRS_STENCILENABLE; static constexpr DWORD value=0; };
struct BothSides { static constexpr D3DRENDERSTATETYPE state=D3DRS_CULLMODE; static constexpr DWORD value=D3DCULL_NONE; };
struct Blend { static constexpr D3DRENDERSTATETYPE state=D3DRS_ALPHABLENDENABLE; static constexpr DWORD value=1; };
struct SourceAlpha { static constexpr D3DRENDERSTATETYPE state=D3DRS_SRCBLEND; static constexpr DWORD value=D3DBLEND_SRCALPHA; };
struct DestinationAlpha { static constexpr D3DRENDERSTATETYPE state=D3DRS_DESTBLEND; static constexpr DWORD value=D3DBLEND_INVSRCALPHA; };
struct Cutout { static constexpr D3DRENDERSTATETYPE state=D3DRS_ALPHATESTENABLE; static constexpr DWORD value=1; };
struct CutoutCompare { static constexpr D3DRENDERSTATETYPE state=D3DRS_ALPHAFUNC; static constexpr DWORD value=D3DCMP_GREATER; };
struct CutoutThreshold { static constexpr D3DRENDERSTATETYPE state=D3DRS_ALPHAREF; static constexpr DWORD value=1; };
struct ColorMask { static constexpr D3DRENDERSTATETYPE state=D3DRS_COLORWRITEENABLE; static constexpr DWORD value=15; };
struct LinearOutput { static constexpr D3DRENDERSTATETYPE state=D3DRS_SRGBWRITEENABLE; static constexpr DWORD value=0; };
struct BlendAdd { static constexpr D3DRENDERSTATETYPE state=D3DRS_BLENDOP; static constexpr DWORD value=D3DBLENDOP_ADD; };
struct UnifiedAlpha { static constexpr D3DRENDERSTATETYPE state=D3DRS_SEPARATEALPHABLENDENABLE; static constexpr DWORD value=0; };
struct NoNativeScissor { static constexpr D3DRENDERSTATETYPE state=D3DRS_SCISSORTESTENABLE; static constexpr DWORD value=0; };
struct Setting { D3DRENDERSTATETYPE state; DWORD value; };
inline constexpr std::array<Setting,18> states = {{
{DepthTest::state,DepthTest::value},
{PreserveNativeDepth::state,PreserveNativeDepth::value},
{DepthCompare::state,DepthCompare::value},
{NoFixedLights::state,NoFixedLights::value},
{NoFog::state,NoFog::value},
{NoStencil::state,NoStencil::value},
{BothSides::state,BothSides::value},
{Blend::state,Blend::value},
{SourceAlpha::state,SourceAlpha::value},
{DestinationAlpha::state,DestinationAlpha::value},
{Cutout::state,Cutout::value},
{CutoutCompare::state,CutoutCompare::value},
{CutoutThreshold::state,CutoutThreshold::value},
{ColorMask::state,ColorMask::value},
{LinearOutput::state,LinearOutput::value},
{BlendAdd::state,BlendAdd::value},
{UnifiedAlpha::state,UnifiedAlpha::value},
{NoNativeScissor::state,NoNativeScissor::value},
}};
}