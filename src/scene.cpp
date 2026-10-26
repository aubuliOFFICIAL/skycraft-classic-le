#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "scene.h"
#include "scene.generated.h"
#include <array>
#include <cmath>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_set>
#include <unordered_map>
namespace classic::scene {
namespace {
namespace l=layout;
bool Bytes(std::uint32_t at,void* out,size_t size) {
    SIZE_T done=0;
    return at && size<=1024*1024 && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),out,size,&done) && done==size;
}
template<class T> bool Read(std::uint32_t at,T& out) { return Bytes(at,&out,sizeof(out)); }
std::uint32_t Ptr(std::uint32_t at) { std::uint32_t out=0;Read(at,out);return out; }
std::vector<std::string> Types(std::uint32_t object) {
    auto fn=Ptr(Ptr(object)+8);std::array<unsigned char,16> code{};
    if(!Bytes(fn,code.data(),code.size())) return {};
    for(unsigned i=0;i<3 && code[0]==0xe9;++i) {
        std::int32_t relative;std::memcpy(&relative,code.data()+1,4);
        fn+=5+relative;if(!Bytes(fn,code.data(),code.size())) return {};
    }
    std::uint32_t type=0;
    if(code[0]==0xb8 && code[5]==0xc3) std::memcpy(&type,code.data()+1,4);
    else if(code[0]==0x8d && code[1]==0x05 && code[6]==0xc3) std::memcpy(&type,code.data()+2,4);
    std::vector<std::string> out;
    for(unsigned i=0;type && i<12;++i) {
        char name[64]{};if(!Bytes(Ptr(type),name,sizeof(name))) break;
        if(!std::memchr(name,0,sizeof(name))) break;
        out.emplace_back(name);type=Ptr(type+4);
    }
    return out;
}
bool Has(const std::vector<std::string>& types,const char* name) {
    for(const auto& t:types) if(t==name) return true;return false;
}
}
Scan ReadVisualGeometry(std::uint32_t data) {
    Scan result;std::deque<std::uint32_t> queue;
    for(unsigned offset:{4u,8u,12u}) if(auto root=Ptr(data+offset)) queue.push_back(root);
    std::unordered_set<std::uint32_t> seen;
    while(!queue.empty() && seen.size()<l::MaximumNodes::value) {
        auto node=queue.front();queue.pop_front();if(!seen.insert(node).second) continue;
        ++result.nodes;auto types=Types(node);
        if(Has(types,"NiNode")) {
            struct Children { std::uint32_t table,data;std::uint16_t capacity,end,size,grow; } children{};
            if(Read(node+l::NodeChildren::value,children) && children.end<=children.capacity && children.end<4096) {
                std::vector<std::uint32_t> refs(children.end);
                if(Bytes(children.data,refs.data(),refs.size()*4)) for(auto child:refs) if(child) queue.push_back(child);
            }
        }
        if(!Has(types,"NiTriShape")) continue;
        if(Ptr(node+l::SkinInstance::value)) { ++result.skinned;continue; }
        auto geometry=Ptr(node+l::GeometryData::value);
        std::uint16_t nv=0,nt=0;std::uint32_t count=0;std::array<float,13> xf{};
        if(!Read(geometry+l::VertexCount::value,nv) || !Read(geometry+l::TriangleCount::value,nt) ||
           !Read(geometry+l::TriangleListLength::value,count) || !Read(node+l::WorldTransform::value,xf) ||
           !nv || !nt || count!=unsigned(nt)*3) { ++result.rejected;continue; }
        bool valid=true;for(auto value:xf) valid&=std::isfinite(value);
        if(!valid || std::abs(xf[12])>1000) { ++result.rejected;continue; }
        std::vector<std::array<float,3>> vertices(nv);std::vector<std::uint16_t> indices(count);
        if(!Bytes(Ptr(geometry+l::Vertices::value),vertices.data(),vertices.size()*sizeof(vertices[0])) ||
           !Bytes(Ptr(geometry+l::TriangleIndices::value),indices.data(),indices.size()*2)) { ++result.rejected;continue; }
        for(auto index:indices) valid&=index<nv;
        for(auto& v:vertices) for(auto value:v) valid&=std::isfinite(value);
        if(!valid) { ++result.rejected;continue; }
        if(result.triangles.size()+nt>l::MaximumTriangles::value) { result.truncated=true;break; }
        ++result.shapes;
        for(unsigned i=0;i<count;i+=3) {
            Triangle tri{};
            for(unsigned j=0;j<3;++j) {
                const auto& v=vertices[indices[i+j]];
                for(unsigned axis=0;axis<3;++axis)
                    tri.v[j*3+axis]=xf[9+axis]+xf[12]*(xf[axis*3]*v[0]+xf[axis*3+1]*v[1]+xf[axis*3+2]*v[2]);
            }
            result.triangles.push_back(tri);
        }
    }
    result.truncated|=!queue.empty();return result;
}
void SetFirstPersonHidden(std::uint32_t root,bool hidden) {
    struct Saved { std::uint32_t vtable,bit; };
    static std::unordered_map<std::uint32_t,Saved> saved;
    // Restore only the bit owned by this adapter, preserving other engine flags.
    auto restore=[](std::uint32_t node,Saved before) {
        std::uint32_t flags=0;if(Ptr(node)!=before.vtable || !Read(node+l::AvFlags::value,flags)) return;
        flags=(flags&~l::AppCulled::value)|before.bit;SIZE_T done;
        WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(node+l::AvFlags::value),&flags,4,&done);
    };
    if(!hidden) { for(const auto& [node,before]:saved) restore(node,before);saved.clear();return; }
    if(!root) return;
    std::deque<std::uint32_t> queue{root};std::unordered_set<std::uint32_t> seen;
    while(!queue.empty() && seen.size()<l::MaximumNodes::value) {
        auto node=queue.front();queue.pop_front();if(!seen.insert(node).second) continue;auto types=Types(node);
        if(Has(types,"NiNode")) {
            struct Children { std::uint32_t table,data;std::uint16_t capacity,end,size,grow; } children{};
            if(Read(node+l::NodeChildren::value,children) && children.end<=children.capacity && children.end<4096) {
                std::vector<std::uint32_t> refs(children.end);
                if(Bytes(children.data,refs.data(),refs.size()*4)) for(auto child:refs) if(child) queue.push_back(child);
            }
        }
        if(!Has(types,"NiGeometry")) continue;
        std::uint32_t flags=0;if(!Read(node+l::AvFlags::value,flags)) continue;
        saved.try_emplace(node,Saved{Ptr(node),flags&l::AppCulled::value});
        flags|=l::AppCulled::value;SIZE_T done;
        WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(node+l::AvFlags::value),&flags,4,&done);
    }
}
}
