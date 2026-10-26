// The mod's own equipment QA recording reads only its actual game framebuffer.
// It never captures a desktop/window, synthesizes imagery, or records audio.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>
#include "capture.h"
#include "capture.generated.h"
namespace classic::capture {
namespace {
std::atomic<ULONGLONG> until{0};ULONGLONG next=0,session=0;unsigned index=0;
IDirect3DSurface9* cpu=nullptr;D3DSURFACE_DESC specNow{};
}
void Arm(){until.store(GetTickCount64()+spec::DurationMs::value,std::memory_order_release);}
void Reset(){if(cpu)cpu->Release();cpu=nullptr;specNow={};}
void Frame(IDirect3DDevice9* device,IDirect3DSurface9* back) {
    const auto end=until.load(std::memory_order_acquire),now=GetTickCount64();
    if(!end || now>end || now<next)return;
    next=now+spec::IntervalMs::value;
    if(session!=end){session=end;index=0;}
    char folder[MAX_PATH]{};if(!GetEnvironmentVariableA("SKYCRAFT_CLASSIC_CAPTURE_DIR",folder,sizeof(folder)))return;
    D3DSURFACE_DESC desc{};if(FAILED(back->GetDesc(&desc)) || !desc.Width || !desc.Height || desc.Width>spec::MaximumWidth::value || desc.Height>spec::MaximumHeight::value ||
        desc.MultiSampleType!=D3DMULTISAMPLE_NONE || (desc.Format!=D3DFMT_A8R8G8B8 && desc.Format!=D3DFMT_X8R8G8B8))return;
    if(!cpu || desc.Width!=specNow.Width || desc.Height!=specNow.Height || desc.Format!=specNow.Format) {
        Reset();if(FAILED(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&cpu,nullptr)))return;specNow=desc;
    }
    if(FAILED(device->GetRenderTargetData(back,cpu)))return;
    D3DLOCKED_RECT lock{};if(FAILED(cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY)))return;
    std::vector<unsigned char> pixels(std::size_t(desc.Width)*desc.Height*4);
    for(unsigned y=0;y<desc.Height;++y)std::memcpy(pixels.data()+std::size_t(y)*desc.Width*4,static_cast<unsigned char*>(lock.pBits)+std::size_t(y)*lock.Pitch,desc.Width*4);
    cpu->UnlockRect();
    BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};
    file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+DWORD(pixels.size());
    info.biSize=sizeof(info);info.biWidth=desc.Width;info.biHeight=-LONG(desc.Height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=DWORD(pixels.size());
    char path[MAX_PATH]{};std::snprintf(path,sizeof(path),"%s\\skyrim-%lu-%llu-%03u.bmp",folder,GetCurrentProcessId(),session,index++);
    HANDLE output=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(output==INVALID_HANDLE_VALUE)return;
    DWORD done=0;WriteFile(output,&file,sizeof(file),&done,nullptr);WriteFile(output,&info,sizeof(info),&done,nullptr);WriteFile(output,pixels.data(),DWORD(pixels.size()),&done,nullptr);CloseHandle(output);
}
}
