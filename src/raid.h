#pragma once
#include <d3d9.h>
namespace classic::raid {
void Update(unsigned remaining,bool resolved,bool reached);
void Draw(IDirect3DDevice9* device);
}
