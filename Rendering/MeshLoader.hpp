#pragma once
#include "MathUtils.hpp"
#include <vector>
#include <cstdint>
struct LoadedMesh
{
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec3> faceNormals;
    std::vector<uint32_t> indices;
    Color color;
    Vec3 boundsMin;
    Vec3 boundsMax;
    Vec3 center;
    float radius = 0;

    int triangleCount() const { return (int)(indices.size() / 3); }
    bool empty() const { return indices.empty(); }
};

LoadedMesh loadMeshFromRaw(
    const double *vertices, size_t vertexCount,
    const uint32_t *faceIndices, size_t faceCount,
    float r, float g, float b, float a = 1.0f);
