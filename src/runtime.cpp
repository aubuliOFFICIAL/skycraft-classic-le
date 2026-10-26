// Independent classic-Skyrim bridge. Diagnostic features remain opt-in.
// No complete movement ownership, encounter or release acceptance yet.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <shlobj.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include "combat.generated.h"
#include "combat_weapons.generated.h"
#include "hooks.generated.h"
#include "render_states.generated.h"
#include "input.generated.h"
#include "native_input.generated.h"
#include "equipment_tests.generated.h"
#include "scene.h"
#include "scene.generated.h"
#include "world_render.h"
#include "capture.h"
#include "helper.h"
#include "device_input.h"
#include "cursor.generated.h"
#include "native_tests.generated.h"
#include "guest_tests.generated.h"
#include "interaction_tests.generated.h"
#include "campaign.generated.h"
#include "native_ui_tests.generated.h"
#include "raid.h"
#include "raid.generated.h"
#include "../upstream/SkyCraft/protocol/skycraft_protocol.h"
namespace {
namespace p = skycraft::proto;
namespace h = classic::hooks;
std::uint8_t* shared = nullptr;
HANDLE mapping = nullptr;
HANDLE hostLease = nullptr;
std::uintptr_t image = 0;
using Present = HRESULT (WINAPI*)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*);
Present originalPresent = nullptr;
using SetDepth=HRESULT(WINAPI*)(IDirect3DDevice9*,IDirect3DSurface9*);
using Reset=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
SetDepth originalSetDepth=nullptr;Reset originalReset=nullptr;
IDirect3DSurface9* cachedDepth=nullptr;
std::mutex depthMutex;
std::atomic<bool> drawingBridge{false};
void** deviceTable = nullptr;
IDirect3DTexture9* overlay = nullptr;
UINT overlayW=0, overlayH=0;
unsigned front=2;
DWORD lastMcPid=0;
bool loggedFrame=false, loggedWorld=false;
bool loggedCallback=false;
bool checkedRenderRestore=false;
std::atomic<bool> overlayEnabled{true};
std::atomic<bool> depthBypassDiagnostic{false};
std::atomic<unsigned> pendingInputTest{0};
std::atomic<unsigned> pendingCombatTest{0};
std::atomic<bool> pendingAimTest{false};
std::atomic<unsigned> pendingInteractionTest{0};
std::atomic<bool> pendingNativeActivation{false};
std::atomic<unsigned> pendingNativeUi{0};
std::unordered_map<unsigned,std::uint32_t> actorPointers;
std::mutex inputMutex;
std::array<bool,classic::input::bindings.size()> lastInput{};
bool routedLastFrame=false;
bool nativeDeathPending=false,sawNativeDeath=false;

void Log(const char* message) {
    char root[MAX_PATH], path[MAX_PATH];
    if (FAILED(SHGetFolderPathA(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,root))) return;
    std::snprintf(path,sizeof(path),"%s\\SkyCraftClassic",root);
    CreateDirectoryA(path,nullptr);
    std::snprintf(path,sizeof(path),"%s\\SkyCraftClassic\\runtime.log",root);
    HANDLE f=CreateFileA(path,FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE) return;
    char line[400]; int n=std::snprintf(line,sizeof(line),"pid=%lu %s\r\n",GetCurrentProcessId(),message);
    DWORD written; WriteFile(f,line,DWORD(n),&written,nullptr); CloseHandle(f);
}

LONG CALLBACK ObserveNativeException(EXCEPTION_POINTERS* details) {
    static std::atomic_flag writing=ATOMIC_FLAG_INIT;
    if(details && details->ExceptionRecord && details->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION && !writing.test_and_set()) {
        HMODULE module=nullptr;char path[MAX_PATH]{},line[300]{};
        auto* record=details->ExceptionRecord;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCSTR>(record->ExceptionAddress),&module);
        if(module)GetModuleFileNameA(module,path,sizeof(path));
        const auto* name=std::strrchr(path,'\\');name=name?name+1:path;
        std::snprintf(line,sizeof(line),"Observed access exception in %s +%08lX, operation=%lu address=%08lX; no memory dump saved",name,static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(record->ExceptionAddress)-reinterpret_cast<std::uintptr_t>(module)),static_cast<unsigned long>(record->ExceptionInformation[0]),static_cast<unsigned long>(record->ExceptionInformation[1]));
        Log(line);writing.clear();
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

HRESULT WINAPI OnSetDepth(IDirect3DDevice9* device,IDirect3DSurface9* surface) {
    const auto result=originalSetDepth(device,surface);
    if(SUCCEEDED(result) && surface && !drawingBridge.load(std::memory_order_relaxed)) {
        IDirect3DSurface9* back=nullptr;D3DSURFACE_DESC d{},b{};
        if(SUCCEEDED(surface->GetDesc(&d)) && SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))) {
            back->GetDesc(&b);back->Release();
            if(std::uint64_t(d.Width)*b.Height==std::uint64_t(d.Height)*b.Width && d.Width>=b.Width/2 && d.Height>=b.Height/2) {
                std::lock_guard<std::mutex> lock(depthMutex);
                if(cachedDepth!=surface) {surface->AddRef();if(cachedDepth)cachedDepth->Release();cachedDepth=surface;}
                static bool logged=false;if(!logged) {char line[180];std::snprintf(line,sizeof(line),"Retained native scene depth %ux%u samples=%u quality=%u",d.Width,d.Height,unsigned(d.MultiSampleType),unsigned(d.MultiSampleQuality));Log(line);logged=true;}
            }
        }
    }
    return result;
}
HRESULT WINAPI OnReset(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* parameters) {
    {std::lock_guard<std::mutex> lock(depthMutex);if(cachedDepth)cachedDepth->Release();cachedDepth=nullptr;}
    classic::world::ResetTargets();
    classic::capture::Reset();
    return originalReset(device,parameters);
}
template<class T> bool Read(std::uintptr_t ptr,T& result) {
    SIZE_T bytes=0;
    return ptr && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(ptr),&result,sizeof(result),&bytes) && bytes==sizeof(result);
}
std::uintptr_t Address(unsigned va) { return image + va-h::PreferredBase::value; }
template<class T> T* At(std::uint64_t offset) { return reinterpret_cast<T*>(shared+offset); }
template<class T> auto Atomic(T& v) { return std::atomic_ref<T>(v); }

bool NativeMenuPaused(){
    unsigned main=0,menus=0,pause=0,item=0,modal=0;unsigned char loadingPause=1;
    if(Read(Address(h::MainSingleton::value),main) && main)Read(main+h::MainPauseOffset::value,loadingPause);
    if(Read(Address(h::MenuManagerSingleton::value),menus) && menus){Read(menus+h::MenuPauseCount::value,pause);Read(menus+h::MenuItemCount::value,item);Read(menus+h::MenuModalCount::value,modal);}
    struct Stack{unsigned data,capacity,count;} stack{};
    if(menus && Read(menus+h::MenuStackOffset::value,stack) && stack.count<=stack.capacity && stack.count<128){
        for(unsigned i=0;i<stack.count;++i){unsigned menu=0,flags=0;if(Read(stack.data+i*4,menu) && menu && Read(menu+h::MenuFlagsOffset::value,flags) && (flags&h::MenuCursorMask::value))return true;}
    }
    return loadingPause || pause || item || modal;
}
bool NativeStoryOwnsPlayer(unsigned player){
    unsigned camera=0,state=0,id=0,flags=0,manager=0,controls=0;
    if(Read(Address(h::PlayerCameraSingleton::value),camera) && camera && Read(camera+h::CameraCurrentState::value,state) && state && Read(state+h::CameraStateId::value,id))
        for(auto native:classic::campaign::Story::nativeCameraStates)if(id==native)return true;
    if(player && Read(player+h::ActorSitSleepFlags::value,flags) && (flags&h::ActorSitSleepMask::value))return true;
    if(Read(Address(h::InputManagerSingleton::value),manager) && manager && Read(manager+h::InputControlState::value,controls) && (controls&h::MovementControlMask::value)!=h::MovementControlMask::value)return true;
    return false;
}
void ActivateNativeTarget(unsigned player){
    // Use the same handler as the player's activation key, including ownership,
    // pickpocket, locked-door and crime decisions. Script Activate bypasses parts
    // of that player interaction path.
    unsigned controls=0,handler=0;unsigned char disabled=1;
    if(!player || !Read(Address(h::ControlsSingleton::value),controls) || !controls ||
       !Read(controls+h::ActivateHandlerOffset::value,handler) || !handler ||
       !Read(handler+h::ActivateDisabledOffset::value,disabled) || disabled)return;
    // Same final short-release action the native handler calls. Includes its
    // crosshair and ownership/crime gate; no script activation fallback.
    using Activate=void(__thiscall*)(void*);
    reinterpret_cast<Activate>(Address(h::PlayerActivateTarget::value))(reinterpret_cast<void*>(player));
    Log("Native player activation action executed; ownership and crime result requires observation");
}
void NativeUiTest(unsigned action){
    for(const auto& test:classic::uitest::tests)if(test.action==action){
        unsigned map=0,context=0,controls=0,source=0;struct Entries{unsigned data,capacity,count;} entries{};
        if(!NativeMenuPaused() || !Read(Address(h::InputManagerSingleton::value),map) || !map || !Read(map+h::InputContextsOffset::value+test.context*4,context) || !context || !Read(context,entries) || entries.count>entries.capacity || entries.count>256 || !Read(Address(h::MenuControlsSingleton::value),controls) || !controls)return;
        Read(Address(h::DeviceManagerSingleton::value),source);
        for(unsigned i=0;i<entries.count;++i){unsigned name=0;char text[64]{};const auto row=entries.data+i*h::InputMappingStride::value;
            if(!Read(row,name) || !name || !Read(name,text) || !std::memchr(text,0,sizeof(text)) || std::strcmp(text,test.event))continue;
            struct Event{unsigned vtable,device,type,next,name,key;float pressure,timer;} event{unsigned(Address(h::ButtonEventVtable::value)),0,0,0,name,test.scan,1,0};static_assert(sizeof(Event)==32);
            auto* ptr=&event;using Receive=unsigned(__thiscall*)(void*,Event**,void*);
            auto receive=reinterpret_cast<Receive>(Address(h::MenuControlsReceive::value));receive(reinterpret_cast<void*>(controls),&ptr,reinterpret_cast<void*>(source));event.pressure=0;event.timer=.1f;ptr=&event;receive(reinterpret_cast<void*>(controls),&ptr,reinterpret_cast<void*>(source));
            Log("Owned QA button sent through native MenuControls; actual menu result requires observation");return;
        }
    }
}

bool PushInput(unsigned kind,unsigned code,int a=0,int b=0,int c=0) {
    const std::lock_guard<std::mutex> lock(inputMutex);
    auto* ring=shared+p::kOffInputRing;
    auto& head=*reinterpret_cast<std::uint64_t*>(ring+p::kInputRingHeadOff);
    auto& tail=*reinterpret_cast<std::uint64_t*>(ring+p::kInputRingTailOff);
    const auto write=Atomic(head).load(std::memory_order_relaxed),read=Atomic(tail).load(std::memory_order_acquire);
    if(write<read || write-read>=p::kInputRingEntries) return false;
    auto* event=reinterpret_cast<p::InputEvent*>(ring+p::kInputRingDataOff)+(write&(p::kInputRingEntries-1));
    *event={std::uint16_t(kind),std::uint16_t(code),a,b,c};
    Atomic(head).store(write+1,std::memory_order_release);return true;
}

HWND inputWindow=nullptr;WNDPROC originalWindowProc=nullptr;
std::atomic<bool> routeMessages{false};
std::array<std::atomic<bool>,classic::input::bindings.size()> messageSeen{},messageDown{},messageTap{};
std::array<std::atomic<bool>,classic::input::bindings.size()> deviceSeen{},deviceDown{},deviceTap{};
std::atomic<long> mouseDeltaX{0},mouseDeltaY{0};float guestCursorX=0,guestCursorY=0;bool hadGuestScreen=false;
void DeviceButton(unsigned i,bool down){deviceSeen[i].store(true);const bool old=deviceDown[i].exchange(down);if(down && !old)deviceTap[i].store(true);}
void FeedKeyboard(const unsigned char* keys){
    if(!shared || !routeMessages.load())return;
    for(unsigned i=0;i<classic::input::bindings.size();++i){const auto& b=classic::input::bindings[i];if(b.kind!=p::kInKey)continue;const auto scan=MapVirtualKeyA(b.vk,MAPVK_VK_TO_VSC);if(scan && scan<256)DeviceButton(i,!ClassicNativeTestActive(scan) && (keys[scan]&0x80)!=0);}
}
void FeedMouse(long x,long y,long wheel,const unsigned char* buttons,unsigned count){
    if(!shared || !routeMessages.load())return;mouseDeltaX.fetch_add(x);mouseDeltaY.fetch_add(y);
    if(wheel)PushInput(p::kInScroll,0,int(wheel));
    for(unsigned i=0;i<classic::input::bindings.size();++i){const auto& b=classic::input::bindings[i];if(b.kind!=p::kInMouseButton)continue;unsigned at=b.vk==VK_LBUTTON?0:b.vk==VK_RBUTTON?1:2;if(at<count)DeviceButton(i,(buttons[at]&0x80)!=0);}
}
void FeedBuffered(unsigned type,unsigned offset,unsigned value){
    if(!shared || !routeMessages.load())return;
    if(type==1)for(unsigned i=0;i<classic::input::bindings.size();++i){const auto& b=classic::input::bindings[i];if(b.kind==p::kInKey && MapVirtualKeyA(b.vk,MAPVK_VK_TO_VSC)==offset)DeviceButton(i,!ClassicNativeTestActive(offset) && (value&0x80)!=0);}
    if(type==2){if(offset==0)mouseDeltaX.fetch_add(long(value));else if(offset==4)mouseDeltaY.fetch_add(long(value));else if(offset==8)PushInput(p::kInScroll,0,int(value));else if(offset>=12 && offset<15)for(unsigned i=0;i<classic::input::bindings.size();++i){const auto& b=classic::input::bindings[i];const unsigned at=b.vk==VK_LBUTTON?0:b.vk==VK_RBUTTON?1:2;if(b.kind==p::kInMouseButton && offset==12+at)DeviceButton(i,(value&0x80)!=0);}}
}
LRESULT CALLBACK InputWindowProc(HWND window,UINT message,WPARAM w,LPARAM l){
    unsigned vk=0;bool down=false,isButton=false;
    if(message==WM_KEYDOWN || message==WM_SYSKEYDOWN){vk=unsigned(w);down=true;}
    else if(message==WM_KEYUP || message==WM_SYSKEYUP)vk=unsigned(w);
    else if(message==WM_LBUTTONDOWN || message==WM_LBUTTONUP){vk=VK_LBUTTON;down=message==WM_LBUTTONDOWN;isButton=true;}
    else if(message==WM_RBUTTONDOWN || message==WM_RBUTTONUP){vk=VK_RBUTTON;down=message==WM_RBUTTONDOWN;isButton=true;}
    else if(message==WM_MBUTTONDOWN || message==WM_MBUTTONUP){vk=VK_MBUTTON;down=message==WM_MBUTTONDOWN;isButton=true;}
    if(vk && routeMessages.load())for(unsigned i=0;i<classic::input::bindings.size();++i){
        const auto& b=classic::input::bindings[i];if(b.vk!=vk || (b.kind==p::kInMouseButton)!=isButton)continue;
        messageSeen[i].store(true);messageDown[i].store(down);if(down)messageTap[i].store(true);
    }
    return CallWindowProcW(originalWindowProc,window,message,w,l);
}

void AttachInputWindow(HWND window){
    if(inputWindow==window || !window)return;
    if(inputWindow && originalWindowProc && reinterpret_cast<WNDPROC>(GetWindowLongPtrW(inputWindow,GWLP_WNDPROC))==InputWindowProc)SetWindowLongPtrW(inputWindow,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(originalWindowProc));
    SetLastError(0);auto previous=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(InputWindowProc)));
    if(previous){originalWindowProc=previous;inputWindow=window;Log("Owned game window input captures brief presses; native window handler preserved");}
}

bool ReadGuest(p::McState& state) {
    auto* src=At<p::McState>(p::kOffMcState);auto seq=Atomic(src->seq);
    for(unsigned i=0;i<16;++i) {
        auto start=seq.load(std::memory_order_acquire);if(start&1) continue;
        std::memcpy(&state,src,sizeof(state));std::atomic_thread_fence(std::memory_order_acquire);
        if(start==seq.load(std::memory_order_relaxed)) return true;
    }
    return false;
}

using CameraTranslation=void(__thiscall*)(void*,void*);
CameraTranslation originalCameraTranslation=nullptr;
void __fastcall OnFirstPersonTranslation(void* cameraState,void*,void* output) {
    originalCameraTranslation(cameraState,output);
    auto* header=At<p::Header>(0);const auto beat=Atomic(header->mcHeartbeatMs).load(std::memory_order_acquire);
    unsigned main=0,mainThread=0,player=0;std::uint8_t paused=1;
    if(!Read(Address(h::MainSingleton::value),main) || !main)return;
    Read(main+h::MainPauseOffset::value,paused);Read(main+h::MainThreadOffset::value,mainThread);
    p::McState mc{};
    if(paused || NativeMenuPaused() || mainThread!=GetCurrentThreadId() || !header->mcPid || !beat || GetTickCount64()-beat>3000 ||
       !ReadGuest(mc) || !(mc.flags&p::kMcInWorld) || (mc.flags&p::kMcDead) ||
       mc.teleportAck!=At<p::SkyState>(p::kOffSkyState)->teleportSeq || !Read(Address(h::PlayerSingleton::value),player) || !player)return;
    if(NativeStoryOwnsPlayer(player))return;
    std::array<float,3> native{};if(!Read(player+h::PlayerPositionOffset::value,native))return;
    const double dx=mc.x-native[0]/p::kUnitsPerBlock,dy=mc.y-native[2]/p::kUnitsPerBlock,dz=mc.z+native[1]/p::kUnitsPerBlock;
    if(!std::isfinite(dx+dy+dz+mc.eyeX+mc.eyeY+mc.eyeZ) || dx*dx+dy*dy+dz*dz>classic::combat::MaximumPoseGapBlocks::value*classic::combat::MaximumPoseGapBlocks::value)return;
    std::array<float,3> eye{float(mc.eyeX*p::kUnitsPerBlock),float(-mc.eyeZ*p::kUnitsPerBlock),float(mc.eyeY*p::kUnitsPerBlock)};
    if(mc.cameraMode && std::isfinite(mc.cameraDistance) && mc.cameraDistance>0){
        std::array<float,3> rotation{};if(Read(player+h::PlayerRotationOffset::value,rotation)){
            const float distance=std::clamp(mc.cameraDistance,0.0f,classic::combat::MaximumCameraDistanceBlocks::value)*p::kUnitsPerBlock;
            eye[0]-=std::sin(rotation[2])*std::cos(rotation[0])*distance;
            eye[1]-=std::cos(rotation[2])*std::cos(rotation[0])*distance;
            eye[2]+=std::sin(rotation[0])*distance;
        }
    }
    std::memcpy(output,eye.data(),sizeof(eye));
}

void AttachCameraTranslation() {
    if(originalCameraTranslation)return;
    unsigned camera=0,first=0,table=0,method=0;
    if(!Read(Address(h::PlayerCameraSingleton::value),camera) || !camera || !Read(camera+h::CameraStates::value,first) || !first ||
       !Read(first,table) || !Read(table+h::CameraTranslationSlot::value*4,method) || method!=Address(h::FirstPersonTranslation::value))return;
    auto* slot=reinterpret_cast<void**>(table)+h::CameraTranslationSlot::value;DWORD protection=0;
    if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection))return;
    originalCameraTranslation=reinterpret_cast<CameraTranslation>(method);
    InterlockedExchangePointer(slot,reinterpret_cast<void*>(&OnFirstPersonTranslation));
    DWORD ignored=0;VirtualProtect(slot,sizeof(void*),protection,&ignored);
    Log("First-person camera translation adapter attached; Minecraft eye alignment requires visual acceptance");
}

void ControlVisualOwnership(std::uint32_t player,bool active) {
    static unsigned ownedCamera=0;static std::uint8_t oldPov=0;
    unsigned cameraNow=0;Read(Address(h::PlayerCameraSingleton::value),cameraNow);
    if(ownedCamera && (!active || cameraNow!=ownedCamera)) {
        std::uint8_t now=1;
        if(Read(ownedCamera+h::CameraAllowTogglePov::value,now) && now==0){SIZE_T done=0;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(ownedCamera+h::CameraAllowTogglePov::value),&oldPov,1,&done);}
        ownedCamera=0;
    }
    if(active && cameraNow) {
        if(!ownedCamera){Read(cameraNow+h::CameraAllowTogglePov::value,oldPov);ownedCamera=cameraNow;}
        // LE copies bDisableAutoVanityMode into this runtime flag at camera
        // initialization. Changing the Setting alone does not update it.
        std::uint8_t disabled=0;SIZE_T done=0;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(cameraNow+h::CameraAllowTogglePov::value),&disabled,1,&done);
    }
    static bool vanityOwned=false;static std::uint8_t oldVanity=0;
    std::uint8_t vanity=0;const auto setting=Address(h::DisableVanitySetting::value)+h::SettingData::value;
    if(Read(setting,vanity)) {
        SIZE_T done=0;
        if(active) {
            if(!vanityOwned) {oldVanity=vanity;vanityOwned=true;}
            const std::uint8_t disable=1;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(setting),&disable,1,&done);
        } else if(vanityOwned) {
            if(vanity==1)WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(setting),&oldVanity,1,&done);
            vanityOwned=false;
        }
    }
    if(active) {
        std::uint32_t camera=0,state=0,id=0,first=0;
        if(Read(Address(h::PlayerCameraSingleton::value),camera) && camera && Read(camera+h::CameraCurrentState::value,state) && state &&
           Read(state+h::CameraStateId::value,id) && id==1 && Read(camera+h::CameraStates::value,first) && first) {
            using SetState=unsigned(__thiscall*)(void*,void*);
            reinterpret_cast<SetState>(Address(h::SetCameraState::value))(reinterpret_cast<void*>(camera),reinterpret_cast<void*>(first));
            Log("Stopped native idle-orbit camera while Minecraft owns aiming");
        }
    }
    static std::uint32_t heldHandler=0,oldEnabled=0,oldVtable=0;
    std::uint32_t controls=0,handler=0,table=0,canProcess=0;
    if(Read(Address(h::ControlsSingleton::value),controls) && controls)
        Read(controls+h::AttackHandlerOffset::value,handler);
    if(heldHandler && (!active || heldHandler!=handler)) {
        std::uint32_t current=0;
        if(Read(heldHandler,current) && current==oldVtable) {SIZE_T done;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(heldHandler+h::HandlerEnabled::value),&oldEnabled,4,&done);}
        heldHandler=0;
    }
    if(active && handler && Read(handler,table) && Read(table+4,canProcess) && canProcess==Address(h::AttackHandlerCanProcess::value)) {
        if(!heldHandler) {Read(handler+h::HandlerEnabled::value,oldEnabled);heldHandler=handler;oldVtable=table;}
        unsigned disabled=0;SIZE_T done;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(handler+h::HandlerEnabled::value),&disabled,4,&done);
    }
    std::uint32_t root=0;if(player) Read(player+h::FirstPersonSkeleton::value,root);
    classic::scene::SetFirstPersonHidden(root,active);
    unsigned loaded=0,body=0;
    if(player && Read(player+classic::scene::layout::LoadedState::value,loaded) && loaded)Read(loaded+classic::scene::layout::LoadedNode::value,body);
    classic::scene::SetFirstPersonHidden(body,active);
}

void ControlMovementOwnership(bool active,bool guestScreen) {
    struct Saved{unsigned pointer=0,table=0,enabled=0;};static std::array<Saved,6> saved{};
    constexpr std::array<unsigned,6> offsets{h::MovementHandlerOffset::value,h::JumpHandlerOffset::value,h::SprintHandlerOffset::value,h::RunHandlerOffset::value,h::SneakHandlerOffset::value,h::LookHandlerOffset::value};
    unsigned controls=0;Read(Address(h::ControlsSingleton::value),controls);
    for(unsigned i=0;i<saved.size();++i) {
        auto& s=saved[i];unsigned current=0,table=0,enabled=0;
        const bool own=active && (i<5 || guestScreen);
        if(controls)Read(controls+offsets[i],current);
        if(s.pointer && (!own || current!=s.pointer)) {
            unsigned nowTable=0,nowEnabled=0;
            if(Read(s.pointer,nowTable) && nowTable==s.table && Read(s.pointer+h::HandlerEnabled::value,nowEnabled) && nowEnabled==0) {
                SIZE_T done=0;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(s.pointer+h::HandlerEnabled::value),&s.enabled,4,&done);
            }
            s={};
        }
        if(own && current && Read(current,table) && table>=image && table<image+h::ExecutableSize::value && Read(current+h::HandlerEnabled::value,enabled)) {
            if(!s.pointer)s={current,table,enabled};
            unsigned zero=0;SIZE_T done=0;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(current+h::HandlerEnabled::value),&zero,4,&done);
        }
    }
}

void ControlNativeBindings(bool active){
    struct Saved{unsigned owned=0,original=0;};static std::array<Saved,classic::nativeinput::bindings.size()> saved{};
    for(unsigned index=0;index<saved.size();++index){
    auto& owned=saved[index].owned;auto& original=saved[index].original;const auto& binding=classic::nativeinput::bindings[index];
    unsigned manager=0,context=0;struct Entries{unsigned data,capacity,count;} entries{};
    unsigned match=0;
    if(Read(Address(h::InputManagerSingleton::value),manager) && manager && Read(manager+h::InputContextsOffset::value,context) && context && Read(context,entries) && entries.count<=entries.capacity && entries.count<256){
        for(unsigned i=0;i<entries.count;++i){
            const unsigned row=entries.data+i*h::InputMappingStride::value;unsigned name=0;char text[64]{};
            if(Read(row,name) && name && Read(name,text) && std::memchr(text,0,sizeof(text)) && !std::strcmp(text,binding.event)){match=row+h::InputMappingButtonOffset::value;break;}
        }
    }
    if(owned && (!active || owned!=match)){
        unsigned now=0;if(Read(owned,now) && now==binding.code){SIZE_T done=0;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(owned),&original,4,&done);}owned=0;
    }
    if(active && match){
        if(!owned){if(!Read(match,original))return;owned=match;Log("Native activation mapped to G in memory; original binding retained for restore");}
        const unsigned key=binding.code;SIZE_T done=0;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(match),&key,4,&done);
    }
    }
}

void ControlHealthOwnership(unsigned player,bool active) {
    static unsigned owned=0,originalBit=0;
    if(owned && (!active || owned!=player)) {
        unsigned flags=0;if(Read(owned+h::ActorFlags2::value,flags)) {
            flags=(flags&~h::EssentialMask::value)|originalBit;SIZE_T done=0;
            WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(owned+h::ActorFlags2::value),&flags,4,&done);
        }
        owned=0;
    }
    if(active && player) {
        unsigned flags=0;if(Read(player+h::ActorFlags2::value,flags)) {
            if(!owned){owned=player;originalBit=flags&h::EssentialMask::value;}
            flags|=h::EssentialMask::value;SIZE_T done=0;
            WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(player+h::ActorFlags2::value),&flags,4,&done);
        }
    }
}

void RouteInput(IDirect3DDevice9* device) {
    auto* header=At<p::Header>(0);
    const auto beat=Atomic(header->mcHeartbeatMs).load(std::memory_order_acquire);
    const bool paused=NativeMenuPaused();
    D3DDEVICE_CREATION_PARAMETERS creation{};device->GetCreationParameters(&creation);
    AttachInputWindow(creation.hFocusWindow);
    const bool route=header->mcPid && beat && GetTickCount64()-beat<3000 &&
        (At<p::SkyState>(p::kOffSkyState)->flags&p::kSkyInGame) && !(At<p::SkyState>(p::kOffSkyState)->flags&(p::kSkyLoading|p::kSkyMenuOpen)) && !paused && GetForegroundWindow()==creation.hFocusWindow;
    routeMessages.store(route);
    if(!route) {
        if(routedLastFrame) PushInput(p::kInReleaseAll,0);
        for(unsigned i=0;i<lastInput.size();++i){messageSeen[i].store(false);messageDown[i].store(false);messageTap[i].store(false);deviceSeen[i].store(false);deviceDown[i].store(false);deviceTap[i].store(false);}
        mouseDeltaX.store(0);mouseDeltaY.store(0);hadGuestScreen=false;
        lastInput.fill(false);routedLastFrame=false;return;
    }
    if(!routedLastFrame) Log("Minecraft keyboard and mouse-button forwarding active; native suppression/movement handoff still pending");
    routedLastFrame=true;
    const bool guestScreen=(At<p::McState>(p::kOffMcState)->flags&p::kMcScreenOpen)!=0;
    const auto* view=At<p::SkyState>(p::kOffSkyState);const auto mx=mouseDeltaX.exchange(0),my=mouseDeltaY.exchange(0);
    if(guestScreen){
        if(!hadGuestScreen){guestCursorX=view->viewportW*.5f;guestCursorY=view->viewportH*.5f;}
        guestCursorX=std::clamp(guestCursorX+float(mx),0.0f,float(view->viewportW));guestCursorY=std::clamp(guestCursorY+float(my),0.0f,float(view->viewportH));
        PushInput(p::kInCursor,0,int(guestCursorX),int(guestCursorY));
    }hadGuestScreen=guestScreen;
    for(size_t i=0;i<lastInput.size();++i) {
        const auto& key=classic::input::bindings[i];
        const bool tap=messageTap[i].exchange(false);
        const bool nativeTap=deviceTap[i].exchange(false);
        bool down=(deviceSeen[i].load() || messageSeen[i].load())?(deviceDown[i].load()||messageDown[i].load()||nativeTap||tap):(GetAsyncKeyState(key.vk)&0x8000)!=0;
        if(key.vk==classic::input::NativeActivate::vk){if(down && !lastInput[i] && !guestScreen)pendingNativeActivation.store(true);lastInput[i]=down;continue;}
        if(down!=lastInput[i]) { if(PushInput(key.kind,key.code,down?1:0)) lastInput[i]=down; }
    }
    // Diagnostic input goes through the same real Minecraft event queue as player input.
    // It proves equipment interaction only, not damage to Skyrim actors.
    static unsigned action=0,phase=0;static ULONGLONG due=0;
    if(!action) {
        action=pendingInputTest.exchange(0);
        if(action) {
            PushInput(p::kInReleaseAll,0);phase=1;due=GetTickCount64()+classic::combat::EquipmentSettleMs::value;
            for(const auto& test:classic::guesttest::tests)if(action==test.action && test.select){PushInput(p::kInKey,test.select,1);PushInput(p::kInKey,test.select,0);}
            if(action<3) {
                unsigned slot=action==1?classic::qa::Sword::slot:classic::qa::Bow::slot;
                PushInput(p::kInKey,slot,1);PushInput(p::kInKey,slot,0);
            }
            Log(action==10?"QA real inventory key input started":action>=3?"QA two-second movement input started":action==1?"QA sword input sequence started":"QA bow input sequence started");
        }
    }
    if(action && GetTickCount64()>=due) {
        unsigned button=action==10?classic::input::Inventory::code:action==5?classic::combat::LeftTestKey::value:action==4?classic::combat::RightTestKey::value:action==3?classic::combat::ForwardTestKey::value:action==1?classic::qa::Sword::mouse:classic::qa::Bow::mouse;
        unsigned kind=action>=3?p::kInKey:p::kInMouseButton;
        unsigned hold=action==10?classic::qa::Sword::holdMs:action>=3?classic::combat::ForwardTestHoldMs::value:action==1?classic::qa::Sword::holdMs:classic::qa::Bow::holdMs;
        for(const auto& test:classic::guesttest::tests)if(action==test.action){button=test.key;kind=test.kind;hold=test.holdMs;}
        unsigned extra=0;for(const auto& test:classic::guesttest::tests)if(action==test.action)extra=test.extraKey;
        if(phase==1) { PushInput(kind,button,1);if(extra)PushInput(p::kInKey,extra,1);phase=2;due=GetTickCount64()+hold; }
        else { PushInput(kind,button,0);if(extra)PushInput(p::kInKey,extra,0);Log("QA input sequence issued completely; visual verification required");action=phase=0; }
    }
}

void PublishActors(std::uint32_t player,std::uint32_t cell) {
    namespace l=classic::scene::layout;
    struct Array { std::uint32_t data,capacity,count; } refs{};
    if(!Read(cell+l::CellReferences::value,refs) || refs.count>refs.capacity || refs.count>20000) return;
    std::vector<std::uint32_t> pointers(refs.count);SIZE_T done=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(refs.data),pointers.data(),pointers.size()*4,&done) || done!=pointers.size()*4) return;
    std::array<float,3> playerPos{};if(!Read(player+h::PlayerPositionOffset::value,playerPos)) return;
    std::vector<p::ActorRecord> records;actorPointers.clear();
    using ValueFn=float(__thiscall*)(void*,unsigned);
    using LevelFn=std::uint16_t(__thiscall*)(void*);
    using HostileFn=bool(__thiscall*)(void*,void*);
    using NameFn=const char*(__thiscall*)(void*);
    for(auto actor:pointers) {
        std::uint8_t type=0;std::uint32_t base=0,loaded=0,node=0;
        std::array<float,3> pos{},rot{};std::array<std::int16_t,6> bounds{};
        if(actor==player || !Read(actor+l::FormType::value,type) || type!=0x3e ||
           !Read(actor+l::BaseForm::value,base) || !base || !Read(base+l::BaseBounds::value,bounds) ||
           !Read(actor+l::LoadedState::value,loaded) || !loaded || !Read(loaded+l::LoadedNode::value,node) || !node ||
           !Read(actor+h::PlayerPositionOffset::value,pos) || !Read(actor+h::PlayerRotationOffset::value,rot)) continue;
        double distance=0;for(unsigned i=0;i<3;++i) distance+=(pos[i]-playerPos[i])*(pos[i]-playerPos[i]);
        if(!std::isfinite(distance) || distance>std::pow(80*p::kUnitsPerBlock,2)) continue;
        auto owner=actor+h::ActorValueOwnerOffset::value;std::uint32_t table=0,current=0,maximum=0;
        if(!Read(owner,table) || !Read(table+4,current) || !Read(table+8,maximum) ||
           current!=Address(h::GetCurrentValue::value) || maximum!=Address(h::GetMaximumValue::value)) continue;
        const float hp=reinterpret_cast<ValueFn>(current)(reinterpret_cast<void*>(owner),h::HealthValue::value);
        const float maxHp=reinterpret_cast<ValueFn>(maximum)(reinterpret_cast<void*>(owner),h::HealthValue::value);
        if(!std::isfinite(hp) || !std::isfinite(maxHp) || maxHp<=0) continue;
        p::ActorRecord r{};if(!Read(actor+h::FormIdOffset::value,r.formId)) continue;
        r.x=pos[0]/p::kUnitsPerBlock;r.y=pos[2]/p::kUnitsPerBlock;r.z=-pos[1]/p::kUnitsPerBlock;
        r.yaw=rot[2]*57.2957795f+180.0f;
        using Bounds=void*(__thiscall*)(void*,void*);
        std::array<float,3> minimum{},maximumBounds{};
        reinterpret_cast<Bounds>(Address(h::GetActorMinBounds::value))(reinterpret_cast<void*>(actor),minimum.data());
        reinterpret_cast<Bounds>(Address(h::GetActorMaxBounds::value))(reinterpret_cast<void*>(actor),maximumBounds.data());
        r.width=std::max(maximumBounds[0]-minimum[0],maximumBounds[1]-minimum[1])/float(p::kUnitsPerBlock);
        r.height=(maximumBounds[2]-minimum[2])/float(p::kUnitsPerBlock);
        if(!std::isfinite(r.width+r.height))continue;
        if(r.width<=0 || r.height<=0) continue; // Do not fabricate missing actor bounds.
        r.width=std::clamp(r.width,0.3f,6.0f);r.height=std::clamp(r.height,0.3f,12.0f);
        using DeadFn=bool(__thiscall*)(void*,bool);
        r.healthFrac=std::clamp(hp/maxHp,0.0f,1.0f);
        if(hp<=0 || reinterpret_cast<DeadFn>(Address(h::IsDead::value))(reinterpret_cast<void*>(actor),true)) r.flags|=p::kActorDead;
        if(reinterpret_cast<HostileFn>(Address(h::IsHostile::value))(reinterpret_cast<void*>(actor),reinterpret_cast<void*>(player))) r.flags|=p::kActorHostile;
        r.level=reinterpret_cast<LevelFn>(Address(h::GetLevel::value))(reinterpret_cast<void*>(actor));
        const auto* name=reinterpret_cast<NameFn>(Address(h::GetReferenceName::value))(reinterpret_cast<void*>(actor));
        if(name) for(unsigned i=0;i<sizeof(r.name)-1;++i) { if(!Read(reinterpret_cast<std::uintptr_t>(name+i),r.name[i]) || !r.name[i]) break; }
        actorPointers[r.formId]=actor;records.push_back(r);if(records.size()==p::kMaxActors) break;
    }
    auto* table=At<p::ActorTable>(p::kOffActorTable);auto seq=Atomic(table->seq);auto next=seq.load(std::memory_order_relaxed);
    seq.store(next+1,std::memory_order_relaxed);std::atomic_thread_fence(std::memory_order_release);
    table->count=unsigned(records.size());if(!records.empty()) std::memcpy(table->actors,records.data(),records.size()*sizeof(p::ActorRecord));
    seq.store(next+2,std::memory_order_release);
    static bool logged=false;if(!logged && !records.empty()) {
        char line[160];std::snprintf(line,sizeof(line),"Published %u native actor records; proxy alignment and visible combat require validation",unsigned(records.size()));Log(line);logged=true;
    }
}

void CombatPlacementProbe(std::uint32_t player,std::uint32_t cell) {
    auto* actors=At<p::ActorTable>(p::kOffActorTable);
    using AiFn=void(__thiscall*)(void*,bool);
    static std::uint32_t heldActor=0,heldForm=0;static ULONGLONG restoreAt=0;
    if(heldActor && GetTickCount64()>=restoreAt) {
        unsigned form=0;if(Read(heldActor+h::FormIdOffset::value,form) && form==heldForm)
            reinterpret_cast<AiFn>(Address(h::EnableAI::value))(reinterpret_cast<void*>(heldActor),true);
        heldActor=heldForm=0;
    }
    if(pendingCombatTest.exchange(0)==1) {
        std::array<float,3> pos{},rot{};Read(player+h::PlayerPositionOffset::value,pos);Read(player+h::PlayerRotationOffset::value,rot);
        std::uint32_t target=0;float closest=1e30f;unsigned form=0;
        for(unsigned i=0;i<actors->count;++i) {
            auto& r=actors->actors[i];if(!(r.flags&p::kActorHostile) || (r.flags&p::kActorDead) || r.height<classic::combat::DiagnosticMinimumHeight::value) continue;
            auto it=actorPointers.find(r.formId);if(it==actorPointers.end()) continue;
            unsigned ai=0;if(!Read(it->second+h::ActorFlagsAI::value,ai) || !(ai&2)) continue;
            std::array<float,3> p2{};if(!Read(it->second+h::PlayerPositionOffset::value,p2)) continue;
            float distance=0;for(unsigned j=0;j<3;++j) distance+=(p2[j]-pos[j])*(p2[j]-pos[j]);
            if(distance<closest) { closest=distance;target=it->second;form=r.formId; }
        }
        if(target) {
            reinterpret_cast<AiFn>(Address(h::EnableAI::value))(reinterpret_cast<void*>(target),false);
            heldActor=target;heldForm=form;restoreAt=GetTickCount64()+8000;
            pos[0]+=std::sin(rot[2])*classic::combat::DiagnosticEnemyDistance::value;
            pos[1]+=std::cos(rot[2])*classic::combat::DiagnosticEnemyDistance::value;
            using MoveFn=void(__cdecl*)(void*,unsigned*,void*,void*,void*,void*);
            unsigned invalid=0;Read(Address(h::InvalidHandle::value),invalid);
            reinterpret_cast<MoveFn>(Address(h::MoveReference::value))(reinterpret_cast<void*>(target),&invalid,reinterpret_cast<void*>(cell),nullptr,pos.data(),rot.data());
            char line[180];std::snprintf(line,sizeof(line),"QA moved existing hostile actor %08X into isolated test room; position %.2f %.2f %.2f",form,pos[0],pos[1],pos[2]);Log(line);
        } else Log("QA found no live hostile actor; no placement performed");
    }
}

// Port of upstream Combat.cpp ApplyHit. LE has a different ABI and HitData layout;
// construct, populate and destroy through the player's own engine, not an SSE SDK.
void ApplyNativeHit(std::uint32_t player,std::uint32_t target,const p::McEvent& event) {
    using DeadFn=bool(__thiscall*)(void*,bool);
    if(reinterpret_cast<DeadFn>(Address(h::IsDead::value))(reinterpret_cast<void*>(target),true))return;
    using ValueFn=float(__thiscall*)(void*,unsigned);
    using LevelFn=std::uint16_t(__thiscall*)(void*);
    const auto level=reinterpret_cast<LevelFn>(Address(h::GetLevel::value))(reinterpret_cast<void*>(target));
    const float damage=event.a*(classic::combat::DamageBaseScale::value+classic::combat::DamageLevelScale::value*level);
    if(!std::isfinite(damage) || damage<=0)return;
    alignas(16) std::array<std::byte,h::HitDataBytes::value> storage{};
    auto* hit=storage.data();
    using Construct=void*(__thiscall*)(void*);
    using Process=void(__thiscall*)(void*,void*);
    using Destroy=void(__thiscall*)(void*);
    Log("Native hit stage: construct");
    reinterpret_cast<Construct>(Address(h::HitDataConstructor::value))(hit);
    auto set=[&](unsigned offset,const auto& value){std::memcpy(hit+offset,&value,sizeof(value));};
    // LE's 00798AC0 is projectile-specific and dereferences its projectile.
    // Do not treat it as SSE's general Populate(attacker,victim,nullptr).
    using Handle=void(__cdecl*)(unsigned*,void*);
    unsigned attackerHandle=0,victimHandle=0;
    reinterpret_cast<Handle>(Address(h::CreateRefHandleByRef::value))(&attackerHandle,reinterpret_cast<void*>(player));
    reinterpret_cast<Handle>(Address(h::CreateRefHandleByRef::value))(&victimHandle,reinterpret_cast<void*>(target));
    set(h::HitAttackerOffset::value,attackerHandle);set(h::HitVictimOffset::value,victimHandle);
    const auto* impact=&classic::impact::weapons[0];
    for(const auto& weapon:classic::impact::weapons)if(weapon.kind==event.weapon)impact=&weapon;
    using Lookup=unsigned(__cdecl*)(unsigned);
    const unsigned weapon=reinterpret_cast<Lookup>(Address(h::LookupFormById::value))(impact->form);
    std::uint8_t type=0;
    if(!weapon || !Read(weapon+classic::scene::layout::FormType::value,type) || type!=h::WeaponFormType::value) {
        reinterpret_cast<Destroy>(Address(h::HitDataDestructor::value))(hit);Log("Native hit rejected: required host weapon impact metadata not found");return;
    }
    set(h::HitWeaponOffset::value,weapon);
    Log("Native hit stage: handles and host impact metadata filled");
    std::array<float,3> targetPos{},sourcePos{};
    Read(target+h::PlayerPositionOffset::value,targetPos);Read(player+h::PlayerPositionOffset::value,sourcePos);
    std::array<float,3> direction{targetPos[0]-sourcePos[0],targetPos[1]-sourcePos[1],0};
    const float length=std::hypot(direction[0],direction[1]);
    if(length>0.001f){direction[0]/=length;direction[1]/=length;}else direction={0,1,0};
    set(h::HitPositionOffset::value,targetPos);set(h::HitDirectionOffset::value,direction);
    set(h::HitDamageOffset::value,damage);set(h::HitPhysicalDamageOffset::value,damage);
    const float zero=0;set(h::HitLimbDamageOffset::value,zero);set(h::HitBlockedDamageOffset::value,zero);
    unsigned flags=0;std::memcpy(&flags,hit+h::HitFlagsOffset::value,sizeof(flags));
    flags&=~(classic::combat::HitCriticalMask::value|classic::combat::HitSneakMask::value|classic::combat::HitPowerMask::value);
    if(event.flags&p::kHitCritical)flags|=classic::combat::HitCriticalMask::value;
    set(h::HitFlagsOffset::value,flags);
    const int skill=(event.flags&p::kHitProjectile)?int(classic::combat::ArcherySkill::value):impact->skill;
    set(h::HitSkillOffset::value,skill);
    const auto owner=target+h::ActorValueOwnerOffset::value;
    const float before=reinterpret_cast<ValueFn>(Address(h::GetCurrentValue::value))(reinterpret_cast<void*>(owner),h::HealthValue::value);
    Log("Native hit stage: process incoming attack");
    reinterpret_cast<Process>(Address(h::ProcessIncomingAttack::value))(reinterpret_cast<void*>(target),hit);
    Log("Native hit stage: destroy");
    reinterpret_cast<Destroy>(Address(h::HitDataDestructor::value))(hit);
    const float after=reinterpret_cast<ValueFn>(Address(h::GetCurrentValue::value))(reinterpret_cast<void*>(owner),h::HealthValue::value);
    char line[220];std::snprintf(line,sizeof(line),"Native hit pipeline handled Minecraft %s event for %08X: %.2f health %.2f -> %.2f; visual acceptance pending",(event.flags&p::kHitProjectile)?"projectile":"melee",event.formId,damage,before,after);Log(line);
}

void ProcessCombatEvents(std::uint32_t player,bool aligned) {
    auto* ring=shared+p::kOffEventRing;
    auto& head=*reinterpret_cast<std::uint64_t*>(ring+p::kEventRingHeadOff);
    auto& tail=*reinterpret_cast<std::uint64_t*>(ring+p::kEventRingTailOff);
    const auto end=Atomic(head).load(std::memory_order_acquire);auto next=Atomic(tail).load(std::memory_order_relaxed);
    if(end<next || end-next>p::kEventRingEntries) { Log("Invalid guest event ring; combat disabled for frame");return; }
    static bool primed=false;
    if(!primed){Atomic(tail).store(end,std::memory_order_release);primed=true;return;}
    for(unsigned budget=0;next<end && budget<32;++next,++budget) {
        const auto event=reinterpret_cast<p::McEvent*>(ring+p::kEventRingDataOff)[next&(p::kEventRingEntries-1)];
        if(event.type==p::kEvPlayerDied && !nativeDeathPending) {
            ControlHealthOwnership(player,false);
            unsigned flags=0;if(Read(player+h::ActorFlags2::value,flags)) {
                flags&=~h::EssentialMask::value;SIZE_T done=0;WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(player+h::ActorFlags2::value),&flags,4,&done);
            }
            using Kill=void(__thiscall*)(void*,void*,float,unsigned,unsigned);
            nativeDeathPending=true;
            using Current=float(__thiscall*)(void*,unsigned);
            const float hp=reinterpret_cast<Current>(Address(h::GetCurrentValue::value))(reinterpret_cast<void*>(player+h::ActorValueOwnerOffset::value),h::HealthValue::value);
            reinterpret_cast<Kill>(Address(h::KillActor::value))(reinterpret_cast<void*>(player),nullptr,std::max(0.0f,hp)+classic::combat::DeathDamageExtra::value,classic::combat::KillPrimaryFlag::value,classic::combat::KillSecondaryFlag::value);
            Log("Real Minecraft death requested native Skyrim death; reload behavior requires acceptance");
            continue;
        }
        if(event.type!=p::kEvHitActor) continue;
        auto found=actorPointers.find(event.formId);if(found==actorPointers.end() || !std::isfinite(event.a) || event.a<=0 || event.a>1000) continue;
        unsigned currentForm=0;
        if(!aligned || !Read(found->second+h::FormIdOffset::value,currentForm) || currentForm!=event.formId)continue;
        ApplyNativeHit(player,found->second,event);
    }
    Atomic(tail).store(next,std::memory_order_release);
    // Preserve truthful attribution: polling sees damage amount, not its source.
    using ValueFn=float(__thiscall*)(void*,unsigned);
    static float previousHealth=-1,pendingDamage=0;static unsigned healthPlayer=0;
    if(healthPlayer!=player){healthPlayer=player;previousHealth=-1;pendingDamage=0;}
    auto owner=player+h::ActorValueOwnerOffset::value;std::uint32_t table=0,get=0,modify=0;
    if(nativeDeathPending){previousHealth=-1;pendingDamage=0;return;}
    if(Read(owner,table) && Read(table+4,get) && get==Address(h::GetCurrentValue::value)) {
        const float hp=reinterpret_cast<ValueFn>(get)(reinterpret_cast<void*>(owner),h::HealthValue::value);
        if(std::isfinite(hp)) {
            if(previousHealth>=0 && hp<previousHealth-0.01f) {
                const float damage=previousHealth-hp;
                if(Read(table+24,modify) && modify==Address(h::ModifyActorValue::value)) {
                    using Modify=void(__thiscall*)(void*,unsigned,unsigned,float);
                    reinterpret_cast<Modify>(modify)(reinterpret_cast<void*>(owner),classic::combat::HealthModifier::value,h::HealthValue::value,damage);
                    pendingDamage+=damage;
                }
            }
            previousHealth=reinterpret_cast<ValueFn>(get)(reinterpret_cast<void*>(owner),h::HealthValue::value);
            const int hundredths=int(std::min(pendingDamage*100,1000000.0f));
            if(hundredths>0 && PushInput(p::kInHurt,p::kHurtOther,hundredths,0,0)){
                pendingDamage-=hundredths/100.0f;
                char line[160];std::snprintf(line,sizeof(line),"Native hit %.2f refunded and forwarded to Minecraft health owner; attacker unknown",hundredths/100.0f);Log(line);
            }
        }
    }
}

void UpdateRaid(unsigned world,unsigned cell,unsigned player){
    using R=classic::raid::spec::EntryChamber;
    if(world!=R::cell){classic::raid::Update(0,false,false);return;}
    using Lookup=unsigned(__cdecl*)(unsigned);using Dead=bool(__thiscall*)(void*,bool);
    auto find=reinterpret_cast<Lookup>(Address(h::LookupFormById::value));unsigned remaining=0;
    for(const unsigned id:R::enemies){
        const unsigned ref=find(id);unsigned currentCell=0,form=0;unsigned char type=0;
        if(!ref || !Read(ref+h::FormIdOffset::value,form) || form!=id || !Read(ref+classic::scene::layout::FormType::value,type) || type!=0x3e || !Read(ref+h::ParentCellOffset::value,currentCell) || currentCell!=cell){classic::raid::Update(0,false,false);return;}
        if(!reinterpret_cast<Dead>(Address(h::IsDead::value))(reinterpret_cast<void*>(ref),true))++remaining;
    }
    const unsigned chest=find(R::chest);unsigned currentCell=0,form=0;unsigned char type=0;std::array<float,3> chestPos{},playerPos{};
    if(!chest || !Read(chest+h::FormIdOffset::value,form) || form!=R::chest || !Read(chest+classic::scene::layout::FormType::value,type) || type!=0x3d || !Read(chest+h::ParentCellOffset::value,currentCell) || currentCell!=cell || !Read(chest+h::PlayerPositionOffset::value,chestPos) || !Read(player+h::PlayerPositionOffset::value,playerPos)){classic::raid::Update(0,false,false);return;}
    const float x=playerPos[0]-chestPos[0],y=playerPos[1]-chestPos[1],z=playerPos[2]-chestPos[2];
    classic::raid::Update(remaining,true,x*x+y*y+z*z<=R::reachUnits*R::reachUnits);
}

void Publish(IDirect3DDevice9* device) {
    p::SkyState state{};
    state.flags=p::kSkyMenuOpen;
    D3DVIEWPORT9 viewport{};
    if(SUCCEEDED(device->GetViewport(&viewport))) {
        state.viewportW=viewport.Width; state.viewportH=viewport.Height;
    }
    std::uint32_t player=0,cell=0,world=0;
    struct Vec { float x,y,z; } pos{},rot{};
    if(Read(Address(h::PlayerSingleton::value),player) && player &&
        Read(player+h::ParentCellOffset::value,cell) && cell &&
        Read(cell+h::FormIdOffset::value,world) && world &&
        Read(player+h::PlayerPositionOffset::value,pos) &&
        Read(player+h::PlayerRotationOffset::value,rot) &&
        std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z)) {
        state.flags=p::kSkyInGame;
        std::uint32_t main=0;std::uint8_t paused=1;
        if(Read(Address(h::MainSingleton::value),main) && main) Read(main+h::MainPauseOffset::value,paused);
        const bool storyOwns=NativeStoryOwnsPlayer(player);
        paused=NativeMenuPaused() || storyOwns;
        if(paused) state.flags|=p::kSkyMenuOpen;
        static bool threadLogged=false,sceneProbed=false;
        std::uint32_t mainThread=0;
        if(main) Read(main+h::MainThreadOffset::value,mainThread);
        if(!threadLogged && mainThread) {
            char line[160];std::snprintf(line,sizeof(line),"Present thread=%lu native main thread=%u; engine calls remain disabled",GetCurrentThreadId(),mainThread);
            Log(line);threadLogged=true;
        }
        static unsigned lastWorld=0,teleportSequence=0;static Vec lastNative{};static bool haveNative=false;
        static bool wasStoryOwned=false;
        if(storyOwns!=wasStoryOwned){if(!storyOwns)++teleportSequence;PushInput(p::kInReleaseAll,0);Log(storyOwns?"Native story or furniture takes the player; Minecraft control yields":"Native story released player; guest teleport acknowledgement required");wasStoryOwned=storyOwns;}
        const float tx=pos.x-lastNative.x,ty=pos.y-lastNative.y,tz=pos.z-lastNative.z;
        const float teleportDistance=classic::combat::NativeTeleportThresholdBlocks::value*p::kUnitsPerBlock;
        if(world!=lastWorld || (haveNative && tx*tx+ty*ty+tz*tz>teleportDistance*teleportDistance)){
            lastWorld=world;++teleportSequence;
            Log("Native cell or displacement requested guest teleport; pose writes await acknowledgement");
        }
        struct CollisionReady{unsigned hostPid,guestPid,world,ready;};
        auto* ready=At<CollisionReady>(h::CollisionReadyOffset::value);
        const bool collisionReady=Atomic(ready->ready).load(std::memory_order_acquire)==1 && ready->hostPid==GetCurrentProcessId() &&
            ready->guestPid==At<p::Header>(0)->mcPid && ready->guestPid && ready->world==world;
        if(!collisionReady)state.flags|=p::kSkyLoading;
        if(mainThread==GetCurrentThreadId()) {
            if(auto action=pendingNativeUi.exchange(0))NativeUiTest(action);
            auto* header=At<p::Header>(0);const auto beat=Atomic(header->mcHeartbeatMs).load(std::memory_order_acquire);p::McState mc{};
            if(nativeDeathPending) {
                using Dead=bool(__thiscall*)(void*,bool);
                const bool dead=reinterpret_cast<Dead>(Address(h::IsDead::value))(reinterpret_cast<void*>(player),true);
                if(dead)sawNativeDeath=true;
                else if(sawNativeDeath){nativeDeathPending=false;sawNativeDeath=false;}
            }
            const bool active=!paused && !nativeDeathPending && header->mcPid && beat && GetTickCount64()-beat<3000 && ReadGuest(mc) && (mc.flags&p::kMcInWorld);
            ControlVisualOwnership(player,active);
            ControlNativeBindings(active);
            ControlHealthOwnership(player,active && !(mc.flags&p::kMcDead));
            char movementMode[8]{};
            const bool movementOwned=GetEnvironmentVariableA("SKYCRAFT_CLASSIC_MOVEMENT_PROBE",movementMode,sizeof(movementMode)) && movementMode[0]=='1';
            ControlMovementOwnership(active && movementOwned,(mc.flags&p::kMcScreenOpen)!=0);
            if(active) {
                unsigned actorState=0;
                if(!(mc.flags&(p::kMcDead|p::kMcScreenOpen|p::kMcSwimming|p::kMcFlying)) &&
                   Read(player+h::ActorSitSleepFlags::value,actorState) &&
                   bool(actorState&h::ActorSneakMask::value)!=bool(mc.flags&p::kMcSneaking)) {
                    using SetSneak=bool(__thiscall*)(void*,bool);
                    reinterpret_cast<SetSneak>(Address(h::SetActorSneaking::value))(reinterpret_cast<void*>(player),(mc.flags&p::kMcSneaking)!=0);
                    Log((mc.flags&p::kMcSneaking)?"Minecraft crouch entered native Skyrim sneak":"Minecraft crouch released native Skyrim sneak");
                }
                if(!(mc.flags&p::kMcScreenOpen) && pendingNativeActivation.exchange(false))ActivateNativeTarget(player);
                const auto aim=pendingInteractionTest.exchange(0);
                for(const auto& test:classic::interactiontest::tests)if(test.action==aim){
                    using Angle=void(__thiscall*)(void*,float);
                    if(!test.reference)reinterpret_cast<Angle>(Address(h::SetPlayerPitch::value))(reinterpret_cast<void*>(player),test.pitch);
                    else {
                        using Lookup=unsigned(__cdecl*)(unsigned);const auto target=reinterpret_cast<Lookup>(Address(h::LookupFormById::value))(test.reference);
                        unsigned targetCell=0;decltype(pos) targetPos{};
                        if(target && Read(target+h::ParentCellOffset::value,targetCell) && targetCell==cell && Read(target+h::PlayerPositionOffset::value,targetPos)){
                            const float dx=targetPos.x-pos.x,dy=targetPos.y-pos.y,dz=targetPos.z+test.height-pos.z-float(mc.eyeHeight*p::kUnitsPerBlock);
                            reinterpret_cast<Angle>(Address(h::SetPlayerYaw::value))(reinterpret_cast<void*>(player),std::atan2(dx,dy));
                            reinterpret_cast<Angle>(Address(h::SetPlayerPitch::value))(reinterpret_cast<void*>(player),-std::atan2(dz,std::hypot(dx,dy)));
                            Log("QA aimed at existing native chest; no placement, teleport, loot or activation fabricated");
                        }
                    }
                }
                static ULONGLONG actorsDue=0;
                if(GetTickCount64()>=actorsDue){actorsDue=GetTickCount64()+100;PublishActors(player,cell);UpdateRaid(world,cell,player);}
                if(pendingAimTest.exchange(false)) {
                    auto* table=At<p::ActorTable>(p::kOffActorTable);const p::ActorRecord* target=nullptr;
                    double closest=classic::combat::AimTestRangeBlocks::value*classic::combat::AimTestRangeBlocks::value;
                    for(unsigned i=0;i<table->count;++i) {
                        const auto& actor=table->actors[i];if(!(actor.flags&p::kActorHostile) || (actor.flags&p::kActorDead))continue;
                        const double x=actor.x-mc.eyeX,y=actor.y+actor.height*classic::combat::AimTestHeightFraction::value-mc.eyeY,z=actor.z-mc.eyeZ;
                        const double distance=x*x+y*y+z*z;if(distance<closest){closest=distance;target=&actor;}
                    }
                    if(target) {
                        const float dx=float(target->x*p::kUnitsPerBlock)-pos.x,dy=float(-target->z*p::kUnitsPerBlock)-pos.y;
                        const float dz=float((target->y+target->height*classic::combat::AimTestHeightFraction::value-mc.eyeY)*p::kUnitsPerBlock);
                        using Angle=void(__thiscall*)(void*,float);
                        reinterpret_cast<Angle>(Address(h::SetPlayerYaw::value))(reinterpret_cast<void*>(player),std::atan2(dx,dy));
                        reinterpret_cast<Angle>(Address(h::SetPlayerPitch::value))(reinterpret_cast<void*>(player),-std::atan2(dz,std::hypot(dx,dy)));
                        Log("QA aimed the player at an existing live hostile; no actor placement, AI change or synthetic hit");
                    }else Log("QA aim found no nearby live hostile; no aiming performed");
                }
                char qa[8]{};
                if(GetEnvironmentVariableA("SKYCRAFT_CLASSIC_COMBAT_PROBE",qa,sizeof(qa)) && qa[0]=='1')CombatPlacementProbe(player,cell);
                double dx=mc.x-pos.x/p::kUnitsPerBlock,dy=mc.y-pos.z/p::kUnitsPerBlock,dz=mc.z+pos.y/p::kUnitsPerBlock;
                const bool finite=std::isfinite(dx+dy+dz);
                char movement[8]{};
                if(collisionReady && GetEnvironmentVariableA("SKYCRAFT_CLASSIC_MOVEMENT_PROBE",movement,sizeof(movement)) && movement[0]=='1' && finite &&
                   mc.teleportAck==teleportSequence && !(mc.flags&p::kMcDead) &&
                   dx*dx+dy*dy+dz*dz<classic::combat::MaximumPoseGapBlocks::value*classic::combat::MaximumPoseGapBlocks::value) {
                    // LE SetPosition updates the actor reference through its engine API.
                    // Physics/controller and camera acceptance remain required in live testing.
                    decltype(pos) guestPos{float(mc.x*p::kUnitsPerBlock),float(-mc.z*p::kUnitsPerBlock),float(mc.y*p::kUnitsPerBlock)};
                    using SetPosition=void(__thiscall*)(void*,void*,bool);
                    reinterpret_cast<SetPosition>(Address(h::SetActorPosition::value))(reinterpret_cast<void*>(player),&guestPos,true);
                    if(Read(player+h::PlayerPositionOffset::value,pos)) {
                        dx=mc.x-pos.x/p::kUnitsPerBlock;dy=mc.y-pos.z/p::kUnitsPerBlock;dz=mc.z+pos.y/p::kUnitsPerBlock;
                    }
                }
                const bool aligned=collisionReady && finite && mc.teleportAck==teleportSequence && dx*dx+dy*dy+dz*dz<=classic::combat::CombatPoseToleranceBlocks::value*classic::combat::CombatPoseToleranceBlocks::value && !(mc.flags&p::kMcDead);
                ProcessCombatEvents(player,aligned);
            }
        }
        char probe[8]{};
        if(!sceneProbed && !paused && mainThread==GetCurrentThreadId() &&
           GetEnvironmentVariableA("SKYCRAFT_CLASSIC_SCENE_PROBE",probe,sizeof(probe)) && probe[0]=='1') {
            sceneProbed=true;std::uint32_t data=0;
            if(Read(cell+classic::scene::layout::CellLoadedData::value,data) && data) {
                const auto scan=classic::scene::ReadVisualGeometry(data);
                char line[200];std::snprintf(line,sizeof(line),"Read-only visual scene probe: nodes=%u shapes=%u triangles=%u rejected=%u truncated=%u; NOT physics collision",scan.nodes,scan.shapes,unsigned(scan.triangles.size()),scan.rejected,unsigned(scan.truncated));
                Log(line);
            }
        }
        state.worldId=world; state.collisionEpoch=world;
        state.posX=pos.x/p::kUnitsPerBlock; state.posY=pos.z/p::kUnitsPerBlock;
        state.posZ=-pos.y/p::kUnitsPerBlock;
        state.yaw=rot.z*57.2957795f+180.0f; state.pitch=rot.x*57.2957795f;
        state.teleportSeq=teleportSequence;
        lastNative=pos;haveNative=true;
        if(!loggedWorld) { Log("Read a live Skyrim player/cell pose; movement ownership remains unfinished"); loggedWorld=true; }
    }
    auto* dst=At<p::SkyState>(p::kOffSkyState);
    auto seq=Atomic(dst->seq); auto next=seq.load(std::memory_order_relaxed);
    seq.store(next+1,std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    std::memcpy(reinterpret_cast<char*>(dst)+4,reinterpret_cast<char*>(&state)+4,sizeof(state)-4);
    seq.store(next+2,std::memory_order_release);
}

void DrawOverlay(IDirect3DDevice9* device) {
    auto* header=At<p::Header>(p::kOffHeader);
    auto pid=Atomic(header->mcPid).load(std::memory_order_acquire);
    if(pid != lastMcPid) {
        lastMcPid=pid; front=2;
        if(pid) Log("Minecraft connected to the classic bridge");
    }
    auto beat=Atomic(header->mcHeartbeatMs).load(std::memory_order_acquire);
    if(!pid || !beat || GetTickCount64()-beat>3000) return;
    auto& ctl=At<p::OverlayCtl>(p::kOffOverlayCtl)->state;
    if(Atomic(ctl).load(std::memory_order_acquire)&p::kOverlayDirty) {
        unsigned acquired=Atomic(ctl).exchange(front,std::memory_order_acq_rel);
        front=acquired&3;
        if(front>=p::kOverlaySlots) { Log("Invalid overlay slot; frame rejected"); front=2; return; }
        p::OverlaySlotHdr frame=*At<p::OverlaySlotHdr>(p::kOffOverlaySlotHdr+front*sizeof(p::OverlaySlotHdr));
        if(!frame.width || !frame.height || frame.width>p::kMaxOverlayW || frame.height>p::kMaxOverlayH) return;
        if(frame.width!=overlayW || frame.height!=overlayH) {
            if(overlay) overlay->Release(); overlay=nullptr;
            overlayW=frame.width; overlayH=frame.height;
            if(FAILED(device->CreateTexture(overlayW,overlayH,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&overlay,nullptr))) return;
        }
        D3DLOCKED_RECT lock{};
        if(overlay && SUCCEEDED(overlay->LockRect(0,&lock,nullptr,0))) {
            const auto* pixels=shared+p::kOffOverlayPixels+p::kOverlaySlotBytes*front;
            for(UINT y=0;y<overlayH;++y) {
                UINT sourceY=(frame.flags&1)?overlayH-1-y:y;
                auto* dest=reinterpret_cast<std::uint32_t*>(static_cast<char*>(lock.pBits)+y*lock.Pitch);
                auto* src=reinterpret_cast<const std::uint32_t*>(pixels+std::uint64_t(sourceY)*overlayW*4);
                for(UINT x=0;x<overlayW;++x) { auto rgba=src[x]; dest[x]=(rgba&0xff00ff00u)|((rgba&255)<<16)|((rgba>>16)&255); }
            }
            overlay->UnlockRect(0);
            if(!loggedFrame) { Log("Received and uploaded a real Minecraft overlay frame"); loggedFrame=true; }
        }
    }
    const auto flags=At<p::SkyState>(p::kOffSkyState)->flags;
    if(!overlay || !overlayEnabled.load(std::memory_order_relaxed) || !(flags&p::kSkyInGame) || (flags&p::kSkyMenuOpen)) return;
    IDirect3DStateBlock9* saved=nullptr;
    if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&saved))) return;
    if(FAILED(saved->Capture())) { saved->Release(); return; }
    std::array<DWORD,classic::hud::states.size()> before{};
    for(size_t i=0;i<before.size();++i) {
        if(FAILED(device->GetRenderState(classic::hud::states[i].state,&before[i]))) { saved->Release(); return; }
    }
    D3DVIEWPORT9 vp{}; device->GetViewport(&vp);
    struct Vertex { float x,y,z,w,u,v; };
    float w=float(vp.Width)-0.5f,ht=float(vp.Height)-0.5f;
    Vertex quad[]={{-0.5f,-0.5f,0,1,0,0},{w,-0.5f,0,1,1,0},{-0.5f,ht,0,1,0,1},{w,ht,0,1,1,1}};
    device->SetVertexShader(nullptr); device->SetPixelShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1); device->SetTexture(0,overlay);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
    device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
    device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
    device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    for(const auto& setting:classic::hud::states) device->SetRenderState(setting.state,setting.value);
    device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
    device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
    device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
    device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
    device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);
    device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
    device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
    device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex));
    if(hadGuestScreen && routeMessages.load()){
        using C=classic::cursor::InventoryPointer;
        struct CursorVertex{float x,y,z,w;unsigned color;};
        const float x=guestCursorX,y=guestCursorY,cw=float(C::width),ch=float(C::height);
        const CursorVertex border[]={{x,y,0,1,C::outline},{x,y+ch,0,1,C::outline},{x+cw,y+ch*.65f,0,1,C::outline}};
        const CursorVertex body[]={{x+2,y+3,0,1,C::color},{x+2,y+ch-3,0,1,C::color},{x+cw-3,y+ch*.65f,0,1,C::color}};
        device->SetTexture(0,nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
        device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
        device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,border,sizeof(CursorVertex));device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,body,sizeof(CursorVertex));
    }
    saved->Apply(); saved->Release();
    bool restored=true;
    for(size_t i=0;i<before.size();++i) {
        DWORD after=0;
        if(FAILED(device->GetRenderState(classic::hud::states[i].state,&after)) || after!=before[i]) {
            restored=false; device->SetRenderState(classic::hud::states[i].state,before[i]);
        }
    }
    if(!checkedRenderRestore) {
        Log(restored ? "HUD drawing restored all 15 audited Skyrim render states" : "HUD state restore mismatch corrected; needs review");
        checkedRenderRestore=true;
    }
}

HRESULT WINAPI OnPresent(IDirect3DDevice9* device,const RECT* source,const RECT* destination,HWND window,const RGNDATA* dirty) {
    const bool first=!loggedCallback;
    if(first) { loggedCallback=true; Log("Final back-buffer presentation callback entered"); }
    IDirect3DSurface9* back=nullptr;
    if(shared && SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))) {
        drawingBridge.store(true,std::memory_order_relaxed);
        IDirect3DSurface9* sceneDepth=nullptr;
        {std::lock_guard<std::mutex> lock(depthMutex);sceneDepth=cachedDepth;if(sceneDepth)sceneDepth->AddRef();}
        IDirect3DStateBlock9* state=nullptr;
        if(SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL,&state)) && SUCCEEDED(state->Capture())) {
            IDirect3DSurface9* targets[4]{}, *depth=nullptr;
            D3DCAPS9 caps{}; device->GetDeviceCaps(&caps);
            UINT count=caps.NumSimultaneousRTs<4?caps.NumSimultaneousRTs:4;
            for(UINT i=0;i<count;++i) device->GetRenderTarget(i,&targets[i]);
            device->GetDepthStencilSurface(&depth);
            device->SetDepthStencilSurface(nullptr);
            for(UINT i=1;i<count;++i) device->SetRenderTarget(i,nullptr);
            if(SUCCEEDED(device->SetRenderTarget(0,back))) {
                D3DSURFACE_DESC desc{};back->GetDesc(&desc);
                D3DVIEWPORT9 viewport{0,0,desc.Width,desc.Height,0.0f,1.0f};device->SetViewport(&viewport);
                Publish(device);RouteInput(device);
                {
                    char worldProbe[8]{};
                    const auto flags=At<p::SkyState>(p::kOffSkyState)->flags;
                    if((flags&p::kSkyInGame) && !(flags&p::kSkyMenuOpen) && GetEnvironmentVariableA("SKYCRAFT_CLASSIC_WORLD_PROBE",worldProbe,sizeof(worldProbe)) && worldProbe[0]=='1') {
                        std::array<float,16> matrix{};
                        if(Read(Address(h::WorldToClip::value),matrix)) classic::world::Draw(device,sceneDepth?sceneDepth:depth,shared,matrix.data(),Log,depthBypassDiagnostic.load(std::memory_order_relaxed));
                    }
                    device->SetDepthStencilSurface(nullptr);
                    if(SUCCEEDED(device->BeginScene())) {DrawOverlay(device);if(classic::campaign::Story::raidOverlay && (flags&p::kSkyInGame) && !(flags&p::kSkyMenuOpen) && !(At<p::McState>(p::kOffMcState)->flags&p::kMcScreenOpen))classic::raid::Draw(device);device->EndScene();}
                }
            }
            device->SetDepthStencilSurface(nullptr);
            if(targets[0]) device->SetRenderTarget(0,targets[0]);
            for(UINT i=1;i<count;++i) device->SetRenderTarget(i,targets[i]);
            device->SetDepthStencilSurface(depth);
            state->Apply();
            for(auto* target:targets) if(target) target->Release();
            if(depth) depth->Release();
        }
        if(state) state->Release();
        if(sceneDepth) sceneDepth->Release();
        drawingBridge.store(false,std::memory_order_relaxed);
        classic::capture::Frame(device,back);
        back->Release();
    }
    if(first) Log("Back-buffer HUD pass finished; original targets restored");
    const HRESULT result=originalPresent(device,source,destination,window,dirty);
    if(first) Log("Original D3D9 Present returned");
    return result;
}

DWORD WINAPI Worker(void*) {
    Sleep(8000); // Let Steam's normal startup and the game's static initialization finish.
    Log("Worker entered after normal game startup");
    image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS*>(image+dos->e_lfanew);
    if(nt->FileHeader.TimeDateStamp!=h::ExecutableStamp::value || nt->OptionalHeader.SizeOfImage!=h::ExecutableSize::value) {
        Log("Unsupported executable; native bridge disabled"); return 0;
    }
    hostLease=CreateMutexW(nullptr,FALSE,L"Local\\SkyCraftClassic_host");
    if(!hostLease) { Log("Could not create host ownership lock"); return 0; }
    const DWORD leaseResult=WaitForSingleObject(hostLease,0);
    if(leaseResult!=WAIT_OBJECT_0 && leaseResult!=WAIT_ABANDONED) { Log("Another classic host owns the bridge; disabled"); CloseHandle(hostLease);hostLease=nullptr;return 0; }
    mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,DWORD(p::kMappingBytes),L"Local\\SkyCraftClassic_v1");
    const bool reused=GetLastError()==ERROR_ALREADY_EXISTS;
    if(!mapping) { Log("Could not create shared mapping"); return 0; }
    shared=static_cast<std::uint8_t*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,SIZE_T(p::kMappingBytes)));
    if(!shared) { Log("Shared memory mapping failed"); return 0; }
    auto* head=At<p::Header>(0);
    if(reused && head->magic==p::kMagic && head->skyrimPid && head->skyrimPid!=GetCurrentProcessId()) {
        HANDLE previous=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,head->skyrimPid);
        const DWORD openError=GetLastError();
        const bool live=previous && WaitForSingleObject(previous,0)==WAIT_TIMEOUT;
        if(previous) CloseHandle(previous);
        if(live || (!previous && openError==ERROR_ACCESS_DENIED)) {
            Log("Previous host is still alive or cannot be checked; not taking its mapping");
            UnmapViewOfFile(shared);shared=nullptr;CloseHandle(mapping);mapping=nullptr;return 0;
        }
    }
    Atomic(head->magic).store(0,std::memory_order_release);
    std::memset(shared+p::kOffSkyState,0,sizeof(p::SkyState));
    std::memset(shared+p::kOffOverlayCtl,0,0x100);
    std::memset(shared+p::kOffInputRing,0,p::kInputRingDataOff);
    std::memset(shared+p::kOffCollisionRing,0,p::kColRingDataOff);
    std::memset(shared+p::kOffActorTable,0,sizeof(p::ActorTable));
    std::memset(shared+p::kOffEventRing,0,p::kEventRingDataOff);
    std::memset(shared+p::kOffWorldEntities,0,sizeof(p::WorldEntities));
    std::memset(shared+p::kOffRenderRing,0,p::kRenRingDataOff);
    head->mcPid=0;head->mcHeartbeatMs=0;head->version=p::kVersion; head->skyrimPid=GetCurrentProcessId();
    head->skyrimHeartbeatMs=GetTickCount64();
    Atomic(head->magic).store(p::kMagic,std::memory_order_release);
    if(reused) Log("Recovered the shared mapping from an ended host");
    Log("Classic runtime started; protocol 11; prototype features are opt-in; no release acceptance pass");
    Log(StartCollisionHelper()?"Owned bundled collision reader started":"Bundled collision reader absent or failed to start; developer helper still required");
    char appData[MAX_PATH],debugConfig[MAX_PATH]{};
    if(SUCCEEDED(SHGetFolderPathA(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,appData))) {
        std::snprintf(debugConfig,sizeof(debugConfig),"%s\\SkyCraftClassic\\debug.ini",appData);
    }
    char testSetting[8]{};GetEnvironmentVariableA("SKYCRAFT_CLASSIC_TEST",testSetting,sizeof(testSetting));
    const bool allowInputTests=testSetting[0]=='1';
    unsigned loops=0,lastTestSequence=0,lastCombatSequence=0;
    for(;;) {
        Atomic(head->skyrimHeartbeatMs).store(GetTickCount64(),std::memory_order_release);
        AttachCameraTranslation();
        if((loops++%20)==0 && debugConfig[0]) {
            unsigned manager=0,object=0,table=0,raw=0;
            if(Read(Address(h::DeviceManagerSingleton::value),manager) && manager){
                if(Read(manager+h::KeyboardObjectOffset::value,object) && object && Read(object,table) && table==Address(h::KeyboardObjectVtable::value) && Read(object+h::KeyboardRawDeviceOffset::value,raw) && raw)AttachClassicDevice(reinterpret_cast<void*>(raw),1);
                if(Read(manager+h::MouseObjectOffset::value,object) && object && Read(object,table) && table==Address(h::MouseObjectVtable::value) && Read(object+h::MouseRawDeviceOffset::value,raw) && raw)AttachClassicDevice(reinterpret_cast<void*>(raw),2);
            }
            overlayEnabled.store(GetPrivateProfileIntA("Debug","DrawOverlay",1,debugConfig)!=0,std::memory_order_relaxed);
            depthBypassDiagnostic.store(allowInputTests && GetPrivateProfileIntA("Test","DisableWorldDepth",classic::combat::DepthBypassDefault::value,debugConfig)!=0,std::memory_order_relaxed);
            unsigned seq=GetPrivateProfileIntA("Test","Sequence",0,debugConfig);
            if(allowInputTests && seq && seq!=lastTestSequence) {
                unsigned action=GetPrivateProfileIntA("Test","Equipment",0,debugConfig);
                if(action>=1 && action<=5) pendingInputTest.store(action);
                if(action==10)pendingInputTest.store(10);
                if(action==13)pendingNativeActivation.store(true);
                for(const auto& test:classic::uitest::tests)if(action==test.action)pendingNativeUi.store(action);
                for(const auto& test:classic::guesttest::tests)if(action==test.action)pendingInputTest.store(action);
                for(const auto& test:classic::interactiontest::tests)if(action==test.action)pendingInteractionTest.store(action);
                for(const auto& test:classic::nativetest::tests)if(action==test.action){RequestClassicNativeTestKey(test.scan,test.hold);Log("Owned QA native key requested; actual native gameplay must confirm its result");}
                if(action==6)pendingAimTest.store(true);
                if(action==8){pendingAimTest.store(true);pendingInputTest.store(1);}
                if(action==9){pendingAimTest.store(true);pendingInputTest.store(2);}
                if(GetPrivateProfileIntA("Test","RecordEquipment",0,debugConfig)==1)classic::capture::Arm();
                lastTestSequence=seq;
            }
            unsigned combatSeq=GetPrivateProfileIntA("Test","CombatSequence",0,debugConfig);
            if(allowInputTests && combatSeq && combatSeq!=lastCombatSequence) {
                if(GetPrivateProfileIntA("Test","CombatMode",0,debugConfig)==1) pendingCombatTest.store(1);
                lastCombatSequence=combatSeq;
            }
        }
        if(!deviceTable) {
            std::uint32_t renderer=0,device=0;
            if(Read(Address(h::RendererSingleton::value),renderer) && renderer &&
                Read(renderer+h::RendererDeviceOffset::value,device) && device) {
                void** original=nullptr;
                if(Read(device,original) && original) {
                    deviceTable=new void*[119];
                    SIZE_T bytes=0;
                    if(!ReadProcessMemory(GetCurrentProcess(),original,deviceTable,119*sizeof(void*),&bytes) || bytes!=119*sizeof(void*)) {
                        delete[] deviceTable; deviceTable=nullptr;
                    } else {
                        originalPresent=reinterpret_cast<Present>(deviceTable[h::PresentSlot::value]);
                        originalSetDepth=reinterpret_cast<SetDepth>(deviceTable[h::SetDepthSlot::value]);
                        originalReset=reinterpret_cast<Reset>(deviceTable[h::ResetSlot::value]);
                        deviceTable[h::PresentSlot::value]=reinterpret_cast<void*>(&OnPresent);
                        MEMORY_BASIC_INFORMATION objectMemory{},methodMemory{};
                        VirtualQuery(reinterpret_cast<void*>(device),&objectMemory,sizeof(objectMemory));
                        VirtualQuery(reinterpret_cast<void*>(originalPresent),&methodMemory,sizeof(methodMemory));
                        const DWORD writeMask=PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
                        const DWORD executeMask=PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
                        if(!(objectMemory.Protect&writeMask) || !(methodMemory.Protect&executeMask)) {
                            Log("Device not initialized or memory protections unexpected; hook deferred");
                            delete[] deviceTable; deviceTable=nullptr;
                            Sleep(500); continue;
                        }
                        Log("Attaching validated D3D9 device vtable");
                        // Patch only the named COM methods; retain extended-device entries.
                        DWORD oldProtect=0;
                        const auto slotBytes=(h::SetDepthSlot::value-h::ResetSlot::value+1)*sizeof(void*);
                        if(!VirtualProtect(original+h::ResetSlot::value,slotBytes,PAGE_READWRITE,&oldProtect)) {
                            Log("Could not make the COM method slot writable; hook disabled");
                            delete[] deviceTable;deviceTable=nullptr;Sleep(500);continue;
                        }
                        InterlockedExchangePointer(reinterpret_cast<void* volatile*>(original+h::SetDepthSlot::value),reinterpret_cast<void*>(&OnSetDepth));
                        InterlockedExchangePointer(reinterpret_cast<void* volatile*>(original+h::ResetSlot::value),reinterpret_cast<void*>(&OnReset));
                        InterlockedExchangePointer(reinterpret_cast<void* volatile*>(original+h::PresentSlot::value),reinterpret_cast<void*>(&OnPresent));
                        DWORD ignored=0;VirtualProtect(original+h::ResetSlot::value,slotBytes,oldProtect,&ignored);
                        Log("Direct3D 9 device attached; host state export enabled");
                    }
                }
            }
        }
        Sleep(50);
    }
}
}
void StartClassicRuntime() {
    AddVectoredExceptionHandler(0,ObserveNativeException);
    HANDLE worker=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);
    if(worker) CloseHandle(worker);
}
void ClassicKeyboardState(const unsigned char* keys){FeedKeyboard(keys);}
void ClassicMouseState(long x,long y,long wheel,const unsigned char* buttons,unsigned count){FeedMouse(x,y,wheel,buttons,count);}
void ClassicBufferedInput(unsigned type,unsigned offset,unsigned value){FeedBuffered(type,offset,value);}
void ClassicInputDiagnostic(const char* message){Log(message);}
