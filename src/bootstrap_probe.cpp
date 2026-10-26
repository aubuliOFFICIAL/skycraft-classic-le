#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <cstdio>
int main() {
    auto dll = LoadLibraryW(L"build\\dinput8.dll");
    if (!dll) { std::printf("Load failed: %lu\n", GetLastError()); return 1; }
    using Create = HRESULT (WINAPI*)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
    auto create = reinterpret_cast<Create>(GetProcAddress(dll,"DirectInput8Create"));
    if (!create) return 2;
    IDirectInput8A* input = nullptr;
    HRESULT hr = create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8A,
        reinterpret_cast<void**>(&input),nullptr);
    if (FAILED(hr) || !input) { std::printf("Forward failed: %lx\n",hr); return 3; }
    input->Release();
    FreeLibrary(dll);
    std::puts("Diagnostic proxy forwards DirectInput successfully in a probe process. Skyrim not started.");
}
