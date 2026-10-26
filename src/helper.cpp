#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include "helpers.generated.h"
#include "helper.h"
bool StartCollisionHelper(){
    wchar_t own[MAX_PATH]{};HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&StartCollisionHelper),&module) || !GetModuleFileNameW(module,own,MAX_PATH))return false;
    std::wstring root=own;auto slash=root.find_last_of(L"\\/");if(slash==std::wstring::npos)return false;root.resize(slash);
    const std::wstring exe=root+L"\\"+classic::helpers::Collision::path;
    if(GetFileAttributesW(exe.c_str())==INVALID_FILE_ATTRIBUTES)return false;
    std::wstring args=L"\""+exe+L"\" "+std::to_wstring(GetCurrentProcessId())+L" "+classic::helpers::Collision::arguments;
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION child{};
    if(!CreateProcessW(exe.c_str(),args.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root.c_str(),&startup,&child))return false;
    CloseHandle(child.hThread);CloseHandle(child.hProcess);return true;
}
