#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <map>
#include <tuple>
#include <algorithm>
#include "building.generated.h"
#include "world_render.h"
#include "world_render_states.generated.h"
#include "world_composite_states.generated.h"
#include "render_states.generated.h"
#include "combat.generated.h"
#include "../upstream/SkyCraft/protocol/skycraft_protocol.h"
namespace classic::world {
namespace {
namespace p=skycraft::proto;
struct Mesh { p::RenScene header{};std::vector<p::RenBatch> batches;std::vector<p::RenVertex> vertices; } scene;
std::map<std::tuple<int,int,int>,Mesh> sections;
size_t sectionVertices=0;
bool HaveMeshes(){return !scene.vertices.empty() || !sections.empty();}
std::unordered_map<unsigned,IDirect3DTexture9*> textures;
unsigned lastGuest=0;
IDirect3DSurface9* sampleTarget=nullptr;
IDirect3DTexture9* resolved=nullptr;
D3DSURFACE_DESC targetSpec{};
void Clear() { for(auto& [id,t]:textures) if(t)t->Release();textures.clear();scene={};sections.clear();sectionVertices=0; }
void Texture(IDirect3DDevice9* d,unsigned id,unsigned w,unsigned h,const std::uint8_t* bytes,size_t size) {
    if(!w || !h || w>8192 || h>8192 || std::uint64_t(w)*h*4>size) return;
    IDirect3DTexture9* t=nullptr;if(FAILED(d->CreateTexture(w,h,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&t,nullptr))) return;
    D3DLOCKED_RECT lock{};
    if(FAILED(t->LockRect(0,&lock,nullptr,0))) {t->Release();return;}
    for(unsigned y=0;y<h;++y) {
        auto* row=reinterpret_cast<unsigned*>(static_cast<char*>(lock.pBits)+y*lock.Pitch);
        for(unsigned x=0;x<w;++x) {unsigned rgba;std::memcpy(&rgba,bytes+(std::uint64_t(y)*w+x)*4,4);row[x]=(rgba&0xff00ff00u)|((rgba&255)<<16)|((rgba>>16)&255);}
    }
    t->UnlockRect(0);auto& old=textures[id];if(old)old->Release();old=t;
}
void Message(IDirect3DDevice9* d,unsigned kind,const std::uint8_t* data,size_t bytes) {
    if(kind==p::kRenAtlas && bytes>=sizeof(p::RenAtlas)) {
        p::RenAtlas h;std::memcpy(&h,data,sizeof(h));Texture(d,0,h.width,h.height,data+sizeof(h),bytes-sizeof(h));
    } else if(kind==p::kRenTexture && bytes>=sizeof(p::RenTexture)) {
        p::RenTexture h;std::memcpy(&h,data,sizeof(h));Texture(d,h.id,h.width,h.height,data+sizeof(h),bytes-sizeof(h));
    } else if(kind==p::kRenAtlasRegion && bytes>=sizeof(p::RenAtlasRegion)) {
        p::RenAtlasRegion h;std::memcpy(&h,data,sizeof(h));auto it=textures.find(0);D3DSURFACE_DESC desc{};
        if(it==textures.end() || FAILED(it->second->GetLevelDesc(0,&desc)) || !h.width || !h.height || h.x>desc.Width || h.width>desc.Width-h.x || h.y>desc.Height || h.height>desc.Height-h.y || sizeof(h)+std::uint64_t(h.width)*h.height*4>bytes)return;
        RECT rect{LONG(h.x),LONG(h.y),LONG(h.x+h.width),LONG(h.y+h.height)};D3DLOCKED_RECT lock{};
        if(SUCCEEDED(it->second->LockRect(0,&lock,&rect,0))){for(unsigned y=0;y<h.height;++y){auto* row=reinterpret_cast<unsigned*>(static_cast<char*>(lock.pBits)+y*lock.Pitch);for(unsigned x=0;x<h.width;++x){unsigned rgba;std::memcpy(&rgba,data+sizeof(h)+(std::uint64_t(y)*h.width+x)*4,4);row[x]=(rgba&0xff00ff00u)|((rgba&255)<<16)|((rgba>>16)&255);}}it->second->UnlockRect(0);}
    } else if(kind==p::kRenClearAll) {scene={};sections.clear();sectionVertices=0;}
    else if(kind==p::kRenSection && bytes>=sizeof(p::RenSection)) {
        p::RenSection h;std::memcpy(&h,data,sizeof(h));const auto key=std::make_tuple(h.sx,h.sy,h.sz);auto old=sections.find(key);const auto prior=old==sections.end()?0:old->second.vertices.size();
        if(!h.vertexCount){if(old!=sections.end()){sectionVertices-=prior;sections.erase(old);}return;}
        if(h.vertexCount%3 || h.vertexCount>classic::building::MaximumSectionVertices::value || sizeof(h)+std::uint64_t(h.vertexCount)*sizeof(p::RenVertex)>bytes || sectionVertices-prior+h.vertexCount>classic::building::MaximumCachedVertices::value || (old==sections.end() && sections.size()>=classic::building::MaximumSections::value))return;
        Mesh next;next.header={double(h.sx)*16,double(h.sy)*16,double(h.sz)*16,1,h.vertexCount};next.vertices.resize(h.vertexCount);next.batches.push_back({0,0,h.vertexCount,0});std::memcpy(next.vertices.data(),data+sizeof(h),next.vertices.size()*sizeof(p::RenVertex));
        for(const auto& v:next.vertices)if(!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z) || !std::isfinite(v.u) || !std::isfinite(v.v))return;
        sectionVertices=sectionVertices-prior+h.vertexCount;sections[key]=std::move(next);
    }
    else if(kind==p::kRenScene && bytes>=sizeof(p::RenScene)) {
        p::RenScene h;std::memcpy(&h,data,sizeof(h));
        const auto need=sizeof(h)+std::uint64_t(h.batchCount)*sizeof(p::RenBatch)+std::uint64_t(h.vertexCount)*sizeof(p::RenVertex);
        if(h.batchCount>4096 || h.vertexCount>1000000 || h.vertexCount%3 || need>bytes ||
           !std::isfinite(h.originX) || !std::isfinite(h.originY) || !std::isfinite(h.originZ)) return;
        Mesh next;next.header=h;next.batches.resize(h.batchCount);next.vertices.resize(h.vertexCount);
        std::memcpy(next.batches.data(),data+sizeof(h),next.batches.size()*sizeof(p::RenBatch));
        std::memcpy(next.vertices.data(),data+sizeof(h)+next.batches.size()*sizeof(p::RenBatch),next.vertices.size()*sizeof(p::RenVertex));
        for(const auto& b:next.batches) if(b.first>h.vertexCount || b.count>h.vertexCount-b.first || b.count%3) return;
        scene=std::move(next);
    }
}
bool Drain(IDirect3DDevice9* d,std::uint8_t* shared,Logger log) {
    auto* header=reinterpret_cast<p::Header*>(shared);
    if(header->mcPid!=lastGuest) {Clear();lastGuest=header->mcPid;}
    auto* ring=shared+p::kOffRenderRing;
    auto& head=*reinterpret_cast<std::uint64_t*>(ring+p::kRenRingHeadOff);
    auto& tail=*reinterpret_cast<std::uint64_t*>(ring+p::kRenRingTailOff);
    const auto end=std::atomic_ref(head).load(std::memory_order_acquire);auto at=std::atomic_ref(tail).load(std::memory_order_relaxed);
    constexpr auto size=p::kRenRingDataBytes;auto* data=ring+p::kRenRingDataOff;
    if(end<at || end-at>size) {log("World renderer rejected corrupt guest ring");return false;}
    auto start=at;
    while(at<end && at-start<(32ull<<20)) {
        const auto pos=at%size;
        if(size-pos<sizeof(p::ColMsgHeader)) return false;
        p::ColMsgHeader h;std::memcpy(&h,data+pos,sizeof(h));
        if(h.type==p::kRenPad) {at+=size-pos;continue;}
        const auto n=(sizeof(h)+std::uint64_t(h.payloadBytes)+7)&~7ull;
        if(n>size-pos || n>end-at) {log("World renderer rejected incomplete guest message");return false;}
        Message(d,h.type,data+pos+sizeof(h),h.payloadBytes);at+=n;
    }
    std::atomic_ref(tail).store(at,std::memory_order_release);return true;
}
struct Vertex {float x,y,z,rhw;unsigned color;float u,v;};
bool Project(const p::RenVertex& v,const p::RenScene& h,const float* m,const D3DVIEWPORT9& port,Vertex& out) {
    const double pos[]{(h.originX+v.x)*p::kUnitsPerBlock,-(h.originZ+v.z)*p::kUnitsPerBlock,(h.originY+v.y)*p::kUnitsPerBlock};
    double clip[4]{};
    for(unsigned row=0;row<4;++row) clip[row]=m[row*4]*pos[0]+m[row*4+1]*pos[1]+m[row*4+2]*pos[2]+m[row*4+3];
    if(!std::isfinite(clip[3]) || clip[3]<0.01) return false;
    const double z=clip[2]/clip[3];if(!std::isfinite(z) || z<0 || z>1) return false;
    out.x=float(port.X+(clip[0]/clip[3]+1)*port.Width*.5-.5);out.y=float(port.Y+(1-clip[1]/clip[3])*port.Height*.5-.5);
    out.z=float(z);out.rhw=float(1/clip[3]);out.u=v.u;out.v=v.v;
    out.color=(v.color&0xff00ff00u)|((v.color&255)<<16)|((v.color>>16)&255);
    return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.u) && std::isfinite(out.v);
}
}
void DrawMeshes(IDirect3DDevice9* device,const D3DVIEWPORT9& port,const float* matrix,Logger log,bool intermediate,bool depthBypass) {
    static bool drawLogged=false;
    if(!HaveMeshes()) return;
    device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
    for(const auto& s:states) device->SetRenderState(s.state,s.value);
    if(depthBypass)device->SetRenderState(DepthTest::state,classic::combat::DepthBypassZValue::value);
    if(intermediate) {
        for(const auto& s:classic::composite::states)device->SetRenderState(s.state,s.value);
    }
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
    device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
    device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
    device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);
    device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
    unsigned drawn=0;
    auto drawMesh=[&](const Mesh& mesh){for(const auto& batch:mesh.batches) {
        auto texture=textures.find(batch.texture);if(texture==textures.end()) continue;
        std::vector<Vertex> vertices;vertices.reserve(batch.count);
        for(unsigned at=batch.first;at<batch.first+batch.count;at+=3) {
            Vertex triangle[3];bool valid=true;for(unsigned i=0;i<3;++i) valid&=Project(mesh.vertices[at+i],mesh.header,matrix,port,triangle[i]);
            if(valid) vertices.insert(vertices.end(),std::begin(triangle),std::end(triangle));
        }
        if(vertices.empty()) continue;device->SetTexture(0,texture->second);
        // Native scene depth is read-only; draw each exported mesh back to front.
        struct Triangle{Vertex v[3];float depth;};std::vector<Triangle> ordered;ordered.reserve(vertices.size()/3);
        for(size_t i=0;i<vertices.size();i+=3)ordered.push_back({{vertices[i],vertices[i+1],vertices[i+2]},(vertices[i].z+vertices[i+1].z+vertices[i+2].z)/3});
        std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.depth>b.depth;});vertices.clear();for(const auto& tri:ordered)vertices.insert(vertices.end(),std::begin(tri.v),std::end(tri.v));
        if(SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,unsigned(vertices.size()/3),vertices.data(),sizeof(Vertex)))) drawn+=unsigned(vertices.size()/3);
    }};
    for(const auto& [key,mesh]:sections)drawMesh(mesh);
    drawMesh(scene);
    if(drawn && !drawLogged) {char line[160];std::snprintf(line,sizeof(line),"Submitted %u actual Minecraft scene triangles using Skyrim projection/depth; visual validation still required",drawn);log(line);drawLogged=true;}
}
void ResetTargets() {
    if(sampleTarget)sampleTarget->Release();sampleTarget=nullptr;
    if(resolved)resolved->Release();resolved=nullptr;targetSpec={};
}
void Draw(IDirect3DDevice9* device,IDirect3DSurface9* depth,std::uint8_t* shared,const float* matrix,Logger log,bool depthBypass) {
    if(!Drain(device,shared,log) || !HaveMeshes()) return;
    static bool depthLogged=false,resolveLogged=false;
    D3DSURFACE_DESC ds{},bs{};IDirect3DSurface9* back=nullptr;
    if(!depth || FAILED(depth->GetDesc(&ds))) {
        if(!depthLogged) {log("No native scene depth available for projectiles; draw skipped");depthLogged=true;}return;
    }
    if(FAILED(device->GetRenderTarget(0,&back))) return;back->GetDesc(&bs);
    static bool specLogged=false;if(!specLogged){char line[180];std::snprintf(line,sizeof(line),"Projectile actual depth %ux%u samples=%u; color %ux%u samples=%u",ds.Width,ds.Height,unsigned(ds.MultiSampleType),bs.Width,bs.Height,unsigned(bs.MultiSampleType));log(line);specLogged=true;}
    D3DVIEWPORT9 screen{};device->GetViewport(&screen);
    if(ds.Width==bs.Width && ds.Height==bs.Height && ds.MultiSampleType==bs.MultiSampleType && SUCCEEDED(device->SetDepthStencilSurface(depth))) {
        if(SUCCEEDED(device->BeginScene())) {DrawMeshes(device,screen,matrix,log,false,depthBypass);device->EndScene();}
        device->SetDepthStencilSurface(nullptr);back->Release();return;
    }
    // Match the real game's depth samples without changing its AA/display settings.
    if(!sampleTarget || ds.Width!=targetSpec.Width || ds.Height!=targetSpec.Height || ds.MultiSampleType!=targetSpec.MultiSampleType || ds.MultiSampleQuality!=targetSpec.MultiSampleQuality) {
        ResetTargets();targetSpec=ds;
        if(FAILED(device->CreateRenderTarget(ds.Width,ds.Height,D3DFMT_A8R8G8B8,ds.MultiSampleType,ds.MultiSampleQuality,FALSE,&sampleTarget,nullptr)) ||
           FAILED(device->CreateTexture(ds.Width,ds.Height,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&resolved,nullptr))) {
            ResetTargets();back->Release();log("Could not allocate a depth-compatible projectile target; draw skipped");return;
        }
    }
    device->SetDepthStencilSurface(nullptr);
    if(FAILED(device->SetRenderTarget(0,sampleTarget)) || FAILED(device->SetDepthStencilSurface(depth))) {
        device->SetDepthStencilSurface(nullptr);device->SetRenderTarget(0,back);back->Release();return;
    }
    D3DVIEWPORT9 viewport{0,0,ds.Width,ds.Height,0,1};device->SetViewport(&viewport);
    // Clear only our color target. Skyrim's depth/stencil is read, never cleared.
    device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);
    if(SUCCEEDED(device->BeginScene())) {DrawMeshes(device,viewport,matrix,log,true,depthBypass);device->EndScene();}
    device->SetDepthStencilSurface(nullptr);device->SetRenderTarget(0,back);device->SetViewport(&screen);
    IDirect3DSurface9* destination=nullptr;HRESULT copied=E_FAIL;
    if(SUCCEEDED(resolved->GetSurfaceLevel(0,&destination))) {
        copied=device->StretchRect(sampleTarget,nullptr,destination,nullptr,D3DTEXF_NONE);destination->Release();
    }
    if(SUCCEEDED(copied) && SUCCEEDED(device->BeginScene())) {
        for(const auto& s:classic::hud::states)device->SetRenderState(s.state,s.value);
        device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
        device->SetTexture(0,resolved);
        device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
        device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
        device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
        const float w=float(screen.Width),h=float(screen.Height);
        const Vertex quad[]={{-.5f,-.5f,0,1,0xffffffff,0,0},{w-.5f,-.5f,0,1,0xffffffff,1,0},{-.5f,h-.5f,0,1,0xffffffff,0,1},{w-.5f,h-.5f,0,1,0xffffffff,1,1}};
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex));device->EndScene();
        if(!resolveLogged) {log("Composited actual guest scene through matching native multisample depth; visual check required");resolveLogged=true;}
    }
    back->Release();
}
}
