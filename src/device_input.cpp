// Keep complete native vtables, including implementation-private methods.
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <unordered_map>
#include <mutex>
#include <cstring>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include "device_input.h"
namespace {
using State=HRESULT(WINAPI*)(void*,DWORD,void*);
using Data=HRESULT(WINAPI*)(void*,DWORD,DIDEVICEOBJECTDATA*,DWORD*,DWORD);
struct Methods{State state=nullptr;Data data=nullptr;};
struct Device{unsigned type=0;bool stateRead=false;bool dataRead=false;void** table=nullptr;Methods methods{};};
std::mutex mutex;std::unordered_map<void**,Methods> tables;std::unordered_map<void*,Device> devices;
std::atomic<unsigned> testScan{0},testStage{0},testHold{0};std::atomic<ULONGLONG> testUntil{0};
Methods GetMethods(void* object){std::lock_guard<std::mutex> guard(mutex);auto it=tables.find(*reinterpret_cast<void***>(object));if(it!=tables.end())return it->second;auto old=devices.find(object);return old==devices.end()?Methods{}:old->second.methods;}
Device GetDevice(void* object){std::lock_guard<std::mutex> guard(mutex);auto it=devices.find(object);return it==devices.end()?Device{}:it->second;}
HRESULT WINAPI ReadState(void* object,DWORD size,void* output){
    const auto methods=GetMethods(object);if(!methods.state){if(output && size<4096)std::memset(output,0,size);return DIERR_INPUTLOST;}
    const auto result=methods.state(object,size,output);const auto p=GetDevice(object);
    if(SUCCEEDED(result) && output){
        if(p.type && !p.stateRead){char message[100];std::snprintf(message,sizeof(message),"Native input state observed: type=%u bytes=%lu",p.type,size);ClassicInputDiagnostic(message);}
        const auto scan=testScan.load();if(p.type==1 && size==256 && scan<256 && scan && testStage.load()==2 && GetTickCount64()<testUntil.load())static_cast<unsigned char*>(output)[scan]|=0x80;
        {std::lock_guard<std::mutex> guard(mutex);auto it=devices.find(object);if(it!=devices.end())it->second.stateRead=true;}
        if(p.type==1 && size==256)ClassicKeyboardState(static_cast<const unsigned char*>(output));
        if(p.type==2 && (size==sizeof(DIMOUSESTATE) || size==sizeof(DIMOUSESTATE2))){const auto* mouse=static_cast<const DIMOUSESTATE2*>(output);ClassicMouseState(mouse->lX,mouse->lY,mouse->lZ,mouse->rgbButtons,size==sizeof(DIMOUSESTATE2)?8:4);}
    }return result;
}
HRESULT WINAPI ReadData(void* object,DWORD stride,DIDEVICEOBJECTDATA* output,DWORD* count,DWORD flags){
    const auto methods=GetMethods(object);if(!methods.data){if(count)*count=0;return DIERR_INPUTLOST;}
    const DWORD capacity=count?*count:0;const auto result=methods.data(object,stride,output,count,flags);const auto p=GetDevice(object);
    if(SUCCEEDED(result) && p.type && !p.dataRead){char message[100];std::snprintf(message,sizeof(message),"Native buffered input observed: type=%u stride=%lu",p.type,stride);ClassicInputDiagnostic(message);std::lock_guard<std::mutex> guard(mutex);devices[object].dataRead=true;}
    if(SUCCEEDED(result) && p.type==1 && output && count && *count<capacity && stride>=16 && !(flags&DIGDD_PEEK)){
        const unsigned stage=testStage.load();const auto now=GetTickCount64();
        if(stage==1 || (stage==2 && now>=testUntil.load())){
            DIDEVICEOBJECTDATA event{};event.dwOfs=testScan.load();event.dwData=stage==1?0x80:0;event.dwTimeStamp=GetTickCount();
            auto* row=reinterpret_cast<unsigned char*>(output)+std::size_t(*count)*stride;std::memset(row,0,stride);std::memcpy(row,&event,std::min<std::size_t>(stride,sizeof(event)));++*count;
            if(stage==1){testUntil.store(now+testHold.load());testStage.store(2);ClassicInputDiagnostic("Owned native test key press emitted through real buffered device input");}
            else{testStage.store(0);ClassicInputDiagnostic("Owned native test key release emitted through real buffered device input");}
        }
    }
    if(SUCCEEDED(result) && output && count && *count<4096 && stride>=16 && !(flags&DIGDD_PEEK) && (!p.stateRead || p.type==1)){
        for(unsigned i=0;i<*count;++i){unsigned values[2]{};std::memcpy(values,reinterpret_cast<const unsigned char*>(output)+i*stride,sizeof(values));ClassicBufferedInput(p.type,values[0],values[1]);}
    }return result;
}
void Replace(void** slot,void* replacement){DWORD previous=0;if(VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&previous)){InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot),replacement);DWORD ignored=0;VirtualProtect(slot,sizeof(void*),previous,&ignored);}}
void Attach(void* object,unsigned type){
    if(!object)return;std::lock_guard<std::mutex> guard(mutex);auto table=*reinterpret_cast<void***>(object);
    if(auto old=devices.find(object);old!=devices.end() && old->second.table==table)return;
    auto& m=tables[table];
    if(!m.state){m.state=reinterpret_cast<State>(table[9]);m.data=reinterpret_cast<Data>(table[10]);Replace(table+9,reinterpret_cast<void*>(&ReadState));Replace(table+10,reinterpret_cast<void*>(&ReadData));}
    devices[object]=Device{type,false,false,table,m};
}
}
void AttachClassicDevice(void* device,unsigned type){if(type==1 || type==2)Attach(device,type);}
void RequestClassicNativeTestKey(unsigned scan,unsigned holdMs){
    char qa[8]{};if(!GetEnvironmentVariableA("SKYCRAFT_CLASSIC_TEST",qa,sizeof(qa)) || qa[0]!='1' || !scan || scan>=256 || holdMs>1000)return;
    testScan.store(scan);testHold.store(holdMs);testStage.store(1);
}
bool ClassicNativeTestActive(unsigned scan){return testStage.load()!=0 && testScan.load()==scan;}
