#pragma once

#include <algorithm>
#include <limits>

#include "gfx_types.h"

struct MeshBounds {
    Vec3 lower{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 upper{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
               std::numeric_limits<float>::lowest()};
    bool valid = false;

    void include(Vec3 point) {
        lower.x = std::min(lower.x, point.x);
        lower.y = std::min(lower.y, point.y);
        lower.z = std::min(lower.z, point.z);
        upper.x = std::max(upper.x, point.x);
        upper.y = std::max(upper.y, point.y);
        upper.z = std::max(upper.z, point.z);
        valid = true;
    }

    Vec3 corner(unsigned index) const {
        return {(index & 1) ? upper.x : lower.x, (index & 2) ? upper.y : lower.y, (index & 4) ? upper.z : lower.z};
    }
};

inline bool cpu_mesh_bounds(const Mesh& mesh, Vec3& lower, Vec3& upper) {
    const float* vertices = mesh.animVertices ? mesh.animVertices : mesh.vertices;
    if (!vertices || mesh.vertexCount <= 0) {
        return false;
    }
    MeshBounds bounds;
    for (int i = 0; i < mesh.vertexCount; ++i) {
        bounds.include({vertices[i * 3], vertices[i * 3 + 1], vertices[i * 3 + 2]});
    }
    lower = bounds.lower;
    upper = bounds.upper;
    return true;
}
