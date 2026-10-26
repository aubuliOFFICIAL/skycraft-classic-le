#pragma once
#include <cstdint>
#include <d3d9.h>
namespace classic::world {
using Logger=void(*)(const char*);
// Candidate renderer: actual guest scene meshes only. No synthetic arrows.
void Draw(IDirect3DDevice9*,IDirect3DSurface9* nativeDepth,std::uint8_t* shared,const float* worldToClip,Logger,bool depthBypass=false);
void ResetTargets();
}
