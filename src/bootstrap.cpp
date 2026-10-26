// Diagnostic first slice only. No gameplay hooks, memory writes or guest launches.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dinput.h>
#include <cstdio>
#include "bootstrap.generated.h"
#include "runtime.h"

namespace {
static_assert(sizeof(void*) * 8 == classic::spec::DiagnosticBootstrap::expectedPointerBits);
HMODULE realInput = nullptr;
INIT_ONCE once = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK Init(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (!n || n + 13 >= MAX_PATH) return FALSE;
    lstrcatW(path, L"\\dinput8.dll");
    realInput = LoadLibraryW(path);
    if (!realInput) return FALSE;
    char root[MAX_PATH], logPath[MAX_PATH];
    DWORD len = GetEnvironmentVariableA("LOCALAPPDATA", root, MAX_PATH);
    if (len && len + 48 < MAX_PATH) {
        std::snprintf(logPath, sizeof(logPath), "%s\\%s", root,
            classic::spec::DiagnosticBootstrap::logDirectory);
        CreateDirectoryA(logPath, nullptr);
        std::snprintf(logPath, sizeof(logPath), "%s\\%s\\%s", root,
            classic::spec::DiagnosticBootstrap::logDirectory,
            classic::spec::DiagnosticBootstrap::logFile);
        HANDLE f = CreateFileA(logPath, FILE_APPEND_DATA, FILE_SHARE_READ,
            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            char line[256];
            int count = std::snprintf(line, sizeof(line),
                "Diagnostic bootstrap loaded; pid=%lu; pointerBits=%u; gameplay=unimplemented\r\n",
                GetCurrentProcessId(), unsigned(sizeof(void*) * 8));
            DWORD written;
            WriteFile(f, line, DWORD(count), &written, nullptr);
            CloseHandle(f);
        }
    }
    StartClassicRuntime(); // Only after the game actually initializes DirectInput, outside loader lock.
    return TRUE;
}
}

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version,
    REFIID iid, LPVOID* output, LPUNKNOWN outer) {
    if (!InitOnceExecuteOnce(&once, Init, nullptr, nullptr)) return E_FAIL;
    using Create = HRESULT (WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    auto create = reinterpret_cast<Create>(GetProcAddress(realInput, "DirectInput8Create"));
    const auto result=create ? create(instance, version, iid, output, outer) : E_FAIL;
    return result;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(instance); }
    return TRUE;
}
