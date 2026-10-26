#pragma once
#include <cstdint>
#include <vector>
namespace classic::scene {
struct Triangle { float v[9]; };
struct Scan {
    unsigned nodes=0, shapes=0, rejected=0, skinned=0;
    bool truncated=false;
    std::vector<Triangle> triangles;
};
// Read-only diagnostic geometry. Visual meshes are NOT assumed to be Havok
// collision: effects, stair helpers and moving bodies require separate handling.
// No guest collision is published by this reader.
Scan ReadVisualGeometry(std::uint32_t loadedCellData);
void SetFirstPersonHidden(std::uint32_t root,bool hidden);
}
