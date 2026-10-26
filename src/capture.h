#pragma once
#include <d3d9.h>
namespace classic::capture {
void Arm();
void Frame(IDirect3DDevice9*,IDirect3DSurface9*);
void Reset();
}
