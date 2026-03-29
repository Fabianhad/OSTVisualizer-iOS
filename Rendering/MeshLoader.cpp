#include "MeshLoader.hpp"

LoadedMesh loadMeshFromRaw(
    const double *vertices, size_t vertexCount,
    const uint32_t *faceIndices, size_t faceCount,
    float r, float g, float b, float a)
{
    LoadedMesh mesh;
    mesh.color = Color(r, g, b, a);

    if (vertexCount == 0 || faceCount == 0)
        return mesh;
    mesh.positions.resize(vertexCount);
    for (size_t i = 0; i < vertexCount; i++)
    {
        mesh.positions[i] = Vec3(
            (float)vertices[i * 3],
            (float)vertices[i * 3 + 1],
            (float)vertices[i * 3 + 2]);
    }
    mesh.indices.resize(faceCount * 3);
    for (size_t i = 0; i < faceCount * 3; i++)
    {
        mesh.indices[i] = faceIndices[i];
    }
    static const float kNormSnap = 2048.0f;
    mesh.normals.resize(vertexCount, Vec3(0, 0, 0));
    mesh.faceNormals.resize(faceCount);
    for (size_t f = 0; f < faceCount; f++)
    {
        uint32_t i0 = mesh.indices[f * 3];
        uint32_t i1 = mesh.indices[f * 3 + 1];
        uint32_t i2 = mesh.indices[f * 3 + 2];

        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
        {
            mesh.faceNormals[f] = Vec3(0, 0, 0);
            continue;
        }

        Vec3 v0 = mesh.positions[i0];
        Vec3 v1 = mesh.positions[i1];
        Vec3 v2 = mesh.positions[i2];

        Vec3 e1 = v1 - v0;
        Vec3 e2 = v2 - v0;
        Vec3 faceNormal = e1.cross(e2);
        mesh.normals[i0] = mesh.normals[i0] + faceNormal;
        mesh.normals[i1] = mesh.normals[i1] + faceNormal;
        mesh.normals[i2] = mesh.normals[i2] + faceNormal;

        float area2 = faceNormal.length();
        if (area2 < 1e-8f)
        {
            mesh.faceNormals[f] = Vec3(0, 0, 0);
        }
        else
        {
            Vec3 n = faceNormal * (1.0f / area2);
            n.x = roundf(n.x * kNormSnap) / kNormSnap;
            n.y = roundf(n.y * kNormSnap) / kNormSnap;
            n.z = roundf(n.z * kNormSnap) / kNormSnap;
            float qLen = n.length();
            mesh.faceNormals[f] = (qLen > 1e-6f) ? n * (1.0f / qLen) : Vec3(0, 0, 0);
        }
    }
    for (auto &n : mesh.normals)
    {
        n = n.normalized();
    }
    mesh.boundsMin = mesh.boundsMax = mesh.positions[0];
    for (size_t i = 1; i < vertexCount; i++)
    {
        const Vec3 &p = mesh.positions[i];
        mesh.boundsMin.x = fminf(mesh.boundsMin.x, p.x);
        mesh.boundsMin.y = fminf(mesh.boundsMin.y, p.y);
        mesh.boundsMin.z = fminf(mesh.boundsMin.z, p.z);
        mesh.boundsMax.x = fmaxf(mesh.boundsMax.x, p.x);
        mesh.boundsMax.y = fmaxf(mesh.boundsMax.y, p.y);
        mesh.boundsMax.z = fmaxf(mesh.boundsMax.z, p.z);
    }
    mesh.center = Vec3(
        (mesh.boundsMin.x + mesh.boundsMax.x) * 0.5f,
        (mesh.boundsMin.y + mesh.boundsMax.y) * 0.5f,
        (mesh.boundsMin.z + mesh.boundsMax.z) * 0.5f);
    mesh.radius = 0;
    for (size_t i = 0; i < vertexCount; i++)
    {
        float d = (mesh.positions[i] - mesh.center).length();
        if (d > mesh.radius)
            mesh.radius = d;
    }

    return mesh;
}
