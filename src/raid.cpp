#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include "raid.h"
#include "raid.generated.h"
#include "render_states.generated.h"
namespace classic::raid {
namespace {std::atomic<unsigned> progress{0};IDirect3DTexture9* panel=nullptr;unsigned rendered=~0u;}
void Update(unsigned remaining,bool resolved,bool reached){progress.store(resolved?(0x100|remaining|(!remaining && reached?0x200:0)):0);}
void Draw(IDirect3DDevice9* device){
    using U=spec::Panel;using R=spec::EntryChamber;const unsigned now=progress.load();if(!(now&0x100))return;
    if(!panel || now!=rendered){
        HDC dc=CreateCompatibleDC(nullptr);if(!dc)return;
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=U::width;info.bmiHeader.biHeight=-LONG(U::height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        HFONT font=CreateFontA(-int(U::fontSize),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");
        if(!bitmap || !font){if(bitmap)DeleteObject(bitmap);if(font)DeleteObject(font);DeleteDC(dc);return;}
        auto oldBitmap=SelectObject(dc,bitmap);auto oldFont=SelectObject(dc,font);PatBlt(dc,0,0,U::width,U::height,BLACKNESS);SetTextColor(dc,RGB(255,255,255));SetBkMode(dc,TRANSPARENT);
        char text[512]{};std::snprintf(text,sizeof(text),"%s  |  Bandits: %u/%u\n%s\n%s",R::title,unsigned(R::enemies.size())-(now&255),unsigned(R::enemies.size()),(now&0x200)?R::completeText:R::briefing,U::controls);
        RECT rect{int(U::padding),int(U::padding),int(U::width-U::padding),int(U::height-U::padding)};DrawTextA(dc,text,-1,&rect,DT_LEFT|DT_NOPREFIX);
        if(!panel)device->CreateTexture(U::width,U::height,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&panel,nullptr);
        D3DLOCKED_RECT lock{};if(panel && SUCCEEDED(panel->LockRect(0,&lock,nullptr,0))){
            for(unsigned y=0;y<U::height;++y){auto* dest=reinterpret_cast<unsigned*>(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch);auto* src=static_cast<unsigned*>(pixels)+y*U::width;for(unsigned x=0;x<U::width;++x){unsigned lum=src[x]&255;dest[x]=lum?((lum<<24)|((((U::textColor>>16)&255)*lum/255)<<16)|((((U::textColor>>8)&255)*lum/255)<<8)|((U::textColor&255)*lum/255)):U::background;}}
            panel->UnlockRect(0);rendered=now;
        }
        SelectObject(dc,oldFont);SelectObject(dc,oldBitmap);DeleteObject(font);DeleteObject(bitmap);DeleteDC(dc);
    }
    if(!panel)return;IDirect3DStateBlock9* saved=nullptr;if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&saved)))return;if(FAILED(saved->Capture())){saved->Release();return;}
    struct V{float x,y,z,w,u,v;};const float x=U::x-.5f,y=U::y-.5f,w=U::width,h=U::height;const V q[]={{x,y,0,1,0,0},{x+w,y,0,1,1,0},{x,y+h,0,1,0,1},{x+w,y+h,0,1,1,1}};
    device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);device->SetTexture(0,panel);
    for(const auto& s:classic::hud::states)device->SetRenderState(s.state,s.value);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(V));saved->Apply();saved->Release();
}
}
