#pragma once

#include <array>
#include <cmath>
#include <span>
#include <vector>

#include "mesh_bounds.h"

inline constexpr unsigned GPU_SKIN_BONES = 64;

inline std::array<std::array<float, 4>, 3> skin_matrix_rows(const Mat4& matrix) {
    return {{{matrix.m0, matrix.m4, matrix.m8, matrix.m12},
             {matrix.m1, matrix.m5, matrix.m9, matrix.m13},
             {matrix.m2, matrix.m6, matrix.m10, matrix.m14}}};
}

class SkinningBounds {
    std::vector<MeshBounds> joints;
    float maximum_weight = 0;

public:
    bool prepare(const Mesh& mesh) {
        joints.clear();
        maximum_weight = 0;
        if (!mesh.vertices || !mesh.boneIndices || !mesh.boneWeights || mesh.boneCount <= 0 ||
            mesh.boneCount > static_cast<int>(GPU_SKIN_BONES)) {
            return false;
        }
        joints.resize(mesh.boneCount);
        for (int vertex = 0; vertex < mesh.vertexCount; ++vertex) {
            const Vec3 point{mesh.vertices[vertex * 3], mesh.vertices[vertex * 3 + 1], mesh.vertices[vertex * 3 + 2]};
            float total = 0;
            for (int influence = 0; influence < 4; ++influence) {
                const unsigned joint = mesh.boneIndices[vertex * 4 + influence];
                const float weight = mesh.boneWeights[vertex * 4 + influence];
                if (!std::isfinite(weight) || weight < 0) {
                    return false;
                }
                if (joint < joints.size() && weight > 0) {
                    joints[joint].include(point);
                    total += weight;
                }
            }
            if (!std::isfinite(total)) {
                return false;
            }
            maximum_weight = std::max(maximum_weight, total);
        }
        return maximum_weight > 0;
    }

    MeshBounds posed(std::span<const Mat4> matrices) const {
        MeshBounds result;
        result.include({0, 0, 0});
        for (size_t joint = 0; joint < joints.size() && joint < matrices.size(); ++joint) {
            if (joints[joint].valid) {
                for (unsigned corner = 0; corner < 8; ++corner) {
                    result.include(vector3_transform(joints[joint].corner(corner), matrices[joint]));
                }
            }
        }
        // A nonnegative weighted sum lies inside this box scaled by the largest weight sum.
        result.lower.x *= maximum_weight;
        result.lower.y *= maximum_weight;
        result.lower.z *= maximum_weight;
        result.upper.x *= maximum_weight;
        result.upper.y *= maximum_weight;
        result.upper.z *= maximum_weight;
        return result;
    }
};
