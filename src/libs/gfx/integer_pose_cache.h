#pragma once

#include <array>
#include <memory>
#include <vector>

#include "mesh_bounds.h"

struct CachedSkinPose {
    std::vector<Transform> pose;
    std::vector<Mat4> matrices;
    std::vector<std::array<float, 4>> position_rows, normal_rows;
    std::vector<MeshBounds> bounds;
};

class IntegerPoseCache {
    const void* animation = nullptr;
    size_t frame_count = 0, bone_count = 0, mesh_count = 0;
    std::vector<std::shared_ptr<const CachedSkinPose>> frames;

public:
    static constexpr size_t budget = 4u << 20;

    bool select(const void* source, size_t count, size_t bones, size_t meshes) {
        const size_t bytes = bones * (sizeof(Transform) + sizeof(Mat4) + 6 * sizeof(std::array<float, 4>)) +
                             meshes * sizeof(MeshBounds) + sizeof(CachedSkinPose) +
                             sizeof(std::shared_ptr<const CachedSkinPose>) + 2 * sizeof(void*);
        if (animation != source || frame_count != count || bone_count != bones || mesh_count != meshes) {
            clear();
            animation = source;
            frame_count = count;
            bone_count = bones;
            mesh_count = meshes;
            if (source && count && bytes && count <= budget / bytes) {
                frames.resize(count);
            }
        }
        return !frames.empty();
    }

    std::shared_ptr<const CachedSkinPose> get(size_t frame) const {
        return frame < frames.size() ? frames[frame] : nullptr;
    }

    void put(size_t frame, std::shared_ptr<const CachedSkinPose> pose) {
        if (frame < frames.size()) {
            frames[frame] = std::move(pose);
        }
    }

    void clear() {
        decltype(frames){}.swap(frames);
        animation = nullptr;
        frame_count = bone_count = mesh_count = 0;
    }
};
