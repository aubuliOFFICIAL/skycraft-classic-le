// Synthetic ABI tests only. Contains no Skyrim content and never opens a game.
#include "scene.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <limits>
template<size_t N,class T> void Put(std::array<unsigned char,N>& dst,size_t offset,const T& value) {
    assert(offset+sizeof(value)<=N);std::memcpy(dst.data()+offset,&value,sizeof(value));
}
template<class T> std::uint32_t Pointer(T* value) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value)); }
int main() {
    static_assert(sizeof(void*)==4);
    std::array<unsigned char,0xC0> node{};std::array<unsigned char,0x60> geometry{};
    const char geometryName[]="NiGeometry";std::uint32_t parentRtti[]{Pointer(geometryName),0};
    const char name[]="NiTriShape";std::uint32_t rtti[]{Pointer(name),Pointer(parentRtti)};
    std::array<unsigned char,16> getter{};getter[0]=0xB8;getter[5]=0xC3;Put(getter,1,Pointer(rtti));
    std::uint32_t vtable[]{0,0,Pointer(getter.data())};Put(node,0,Pointer(vtable));
    std::uint32_t roots[]{0,Pointer(node.data()),Pointer(node.data()),0};
    Put(node,0xB0,Pointer(geometry.data()));
    const std::array<float,13> transform{0,-1,0,1,0,0,0,0,1,10,20,30,2};
    Put(node,0x54,transform);
    std::array<float,9> vertices{1,2,3,4,5,6,7,8,9};std::array<std::uint16_t,3> indices{0,1,2};
    Put(geometry,8,std::uint16_t{3});Put(geometry,0x48,std::uint16_t{1});Put(geometry,0x4C,std::uint32_t{3});
    Put(geometry,0x20,Pointer(vertices.data()));Put(geometry,0x50,Pointer(indices.data()));
    auto scan=classic::scene::ReadVisualGeometry(Pointer(roots));
    assert(scan.nodes==1 && scan.shapes==1 && scan.triangles.size()==1 && !scan.truncated);
    assert(scan.triangles[0].v[0]==6 && scan.triangles[0].v[1]==22 && scan.triangles[0].v[2]==36);
    indices[2]=3;scan=classic::scene::ReadVisualGeometry(Pointer(roots));
    assert(scan.rejected==1 && scan.triangles.empty());indices[2]=2;
    vertices[0]=std::numeric_limits<float>::quiet_NaN();scan=classic::scene::ReadVisualGeometry(Pointer(roots));
    assert(scan.rejected==1 && scan.triangles.empty());vertices[0]=1;
    Put(node,0xB4,std::uint32_t{1});scan=classic::scene::ReadVisualGeometry(Pointer(roots));
    assert(scan.skinned==1 && scan.triangles.empty());Put(node,0xB4,std::uint32_t{0});
    Put(geometry,0x4C,std::uint32_t{0xffffffff});scan=classic::scene::ReadVisualGeometry(Pointer(roots));
    assert(scan.rejected==1 && scan.triangles.empty());
    Put(node,0x98,std::uint32_t{0x80});
    classic::scene::SetFirstPersonHidden(Pointer(node.data()),true);
    std::uint32_t flags=0;std::memcpy(&flags,node.data()+0x98,4);assert(flags==0x81);
    Put(node,0x98,std::uint32_t{0x181}); // Simulate another engine flag changing while hidden.
    classic::scene::SetFirstPersonHidden(Pointer(node.data()),false);
    std::memcpy(&flags,node.data()+0x98,4);assert(flags==0x180);
    std::puts("Scene ABI diagnostic passed: transform, bounds rejection, skin exclusion, reversible visibility bit ownership. Not a gameplay test.");
}
