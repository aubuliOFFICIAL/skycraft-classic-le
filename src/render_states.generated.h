// Generated from sheets/render_states.json. Do not edit.
#pragma once
#include <array>
namespace classic::hud {
struct NoFixedFunctionLighting { static constexpr D3DRENDERSTATETYPE state=D3DRS_LIGHTING; static constexpr DWORD value=0; };
struct NoOverlayFog { static constexpr D3DRENDERSTATETYPE state=D3DRS_FOGENABLE; static constexpr DWORD value=0; };
struct NoAlphaTest { static constexpr D3DRENDERSTATETYPE state=D3DRS_ALPHATESTENABLE; static constexpr DWORD value=0; };
struct NoStencilTest { static constexpr D3DRENDERSTATETYPE state=D3DRS_STENCILENABLE; static constexpr DWORD value=0; };
struct NoOverlayDepth { static constexpr D3DRENDERSTATETYPE state=D3DRS_ZENABLE; static constexpr DWORD value=0; };
struct NoDepthWrites { static constexpr D3DRENDERSTATETYPE state=D3DRS_ZWRITEENABLE; static constexpr DWORD value=0; };
struct TwoSidedOverlay { static constexpr D3DRENDERSTATETYPE state=D3DRS_CULLMODE; static constexpr DWORD value=D3DCULL_NONE; };
struct PremultipliedAlpha { static constexpr D3DRENDERSTATETYPE state=D3DRS_ALPHABLENDENABLE; static constexpr DWORD value=1; };
struct SourceAlphaBlend { static constexpr D3DRENDERSTATETYPE state=D3DRS_SRCBLEND; static constexpr DWORD value=D3DBLEND_ONE; };
struct DestinationAlphaBlend { static constexpr D3DRENDERSTATETYPE state=D3DRS_DESTBLEND; static constexpr DWORD value=D3DBLEND_INVSRCALPHA; };
struct AdditiveBlendEquation { static constexpr D3DRENDERSTATETYPE state=D3DRS_BLENDOP; static constexpr DWORD value=D3DBLENDOP_ADD; };
struct NoSeparateAlpha { static constexpr D3DRENDERSTATETYPE state=D3DRS_SEPARATEALPHABLENDENABLE; static constexpr DWORD value=0; };
struct NoScissor { static constexpr D3DRENDERSTATETYPE state=D3DRS_SCISSORTESTENABLE; static constexpr DWORD value=0; };
struct DisplayEncodedOverlay { static constexpr D3DRENDERSTATETYPE state=D3DRS_SRGBWRITEENABLE; static constexpr DWORD value=0; };
struct WriteRgba { static constexpr D3DRENDERSTATETYPE state=D3DRS_COLORWRITEENABLE; static constexpr DWORD value=15; };
struct Setting { D3DRENDERSTATETYPE state; DWORD value; };
inline constexpr std::array<Setting,15> states = {{
{NoFixedFunctionLighting::state,NoFixedFunctionLighting::value},
{NoOverlayFog::state,NoOverlayFog::value},
{NoAlphaTest::state,NoAlphaTest::value},
{NoStencilTest::state,NoStencilTest::value},
{NoOverlayDepth::state,NoOverlayDepth::value},
{NoDepthWrites::state,NoDepthWrites::value},
{TwoSidedOverlay::state,TwoSidedOverlay::value},
{PremultipliedAlpha::state,PremultipliedAlpha::value},
{SourceAlphaBlend::state,SourceAlphaBlend::value},
{DestinationAlphaBlend::state,DestinationAlphaBlend::value},
{AdditiveBlendEquation::state,AdditiveBlendEquation::value},
{NoSeparateAlpha::state,NoSeparateAlpha::value},
{NoScissor::state,NoScissor::value},
{DisplayEncodedOverlay::state,DisplayEncodedOverlay::value},
{WriteRgba::state,WriteRgba::value},
}};
}