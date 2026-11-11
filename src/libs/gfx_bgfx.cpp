#include "gfx.h"

#include <algorithm>
#include <array>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <cmath>
#include <condition_variable>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <span>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../platform/platform.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "gfx/glyph_lookup.h"
#include "gfx/mesh_bounds.h"
#include "gfx/sdl_compat.h"
#include "shaders/spirv/fs_2d.sc.bin.h"
#include "shaders/spirv/fs_colortransform.sc.bin.h"
#include "shaders/spirv/fs_cutout.sc.bin.h"
#include "shaders/spirv/fs_face.sc.bin.h"
#include "shaders/spirv/fs_mesh.sc.bin.h"
#include "shaders/spirv/fs_null.sc.bin.h"
#include "shaders/spirv/fs_outline.sc.bin.h"
#include "shaders/spirv/fs_outline_fxaa.sc.bin.h"
#include "shaders/spirv/fs_sprite.sc.bin.h"
#include "shaders/spirv/vs_2d.sc.bin.h"
#include "shaders/spirv/vs_mesh.sc.bin.h"
#include "shaders/spirv/vs_mesh_skin.sc.bin.h"
#include "shaders/spirv/vs_outline.sc.bin.h"
#include "shaders/spirv/vs_outline_skin.sc.bin.h"
#include "shaders/spirv/vs_sprite.sc.bin.h"

namespace {

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
                const Vec3 point{mesh.vertices[vertex * 3], mesh.vertices[vertex * 3 + 1],
                                 mesh.vertices[vertex * 3 + 2]};
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

    inline std::array<float, 3> sprite_rgb_to_hsv(const std::array<float, 4>& color) {
        const float r = color[0], g = color[1], b = color[2];
        const std::array<float, 4> p =
            g >= b ? std::array{g, b, 0.0f, -1.0f / 3.0f} : std::array{b, g, -1.0f, 2.0f / 3.0f};
        const std::array<float, 4> q = r >= p[0] ? std::array{r, p[1], p[2], p[0]} : std::array{p[0], p[1], p[3], r};
        const float d = q[0] - std::min(q[3], q[1]);
        constexpr float epsilon = 1.0e-10f;
        return {std::abs(q[2] + (q[3] - q[1]) / (6.0f * d + epsilon)), d / (q[0] + epsilon), q[0]};
    }

    inline std::array<float, 4> sprite_color_transform(const std::array<float, 4>& source,
                                                       const std::array<float, 4>& target) {
        const auto from = sprite_rgb_to_hsv(source), to = sprite_rgb_to_hsv(target);
        return {to[0] - from[0], from[1] > 0 ? to[1] / from[1] : 1.0f, from[2] > 0 ? to[2] / from[2] : 1.0f, 1.0f};
    }

    struct alignas(64) SpriteInstance {
        std::array<float, 4> rectangle;
        std::array<float, 4> uv;
        std::array<float, 4> origin_rotation;
        std::array<float, 4> color_transform;

        static SpriteInstance make(Rect destination, std::array<float, 4> uv, Vec2 origin, float rotation, Color tint,
                                   uint32_t texture_index, std::array<float, 4> transform) {
            // Integer-valued floats preserve every packed bit without relying on NaN payloads.
            const uint32_t rg =
                tint.r | (uint32_t{tint.g} << 8) | (texture_index << 16) | (uint32_t{transform[3] > 0.5f} << 19);
            transform[3] = static_cast<float>(tint.b | (uint32_t{tint.a} << 8));
            return {{destination.x, destination.y, destination.width, destination.height},
                    uv,
                    {origin.x, origin.y, rotation, static_cast<float>(rg)},
                    transform};
        }
    };

    static_assert(sizeof(SpriteInstance) == 64);

    class SpriteTextures {
        std::array<unsigned, 8> textures{};
        unsigned used = 0;

    public:
        static constexpr unsigned capacity = 8;
        unsigned size() const { return used; }
        unsigned operator[](unsigned index) const { return textures[index]; }
        bool contains(unsigned texture) const {
            return std::find(textures.begin(), textures.begin() + used, texture) != textures.begin() + used;
        }
        void clear() { used = 0; }

        template<class Flush>
        unsigned add(unsigned texture, Flush flush) {
            for (unsigned i = 0; i < used; ++i) {
                if (textures[i] == texture) {
                    return i;
                }
            }
            if (used == capacity) {
                flush();
                clear();
            }
            textures[used] = texture;
            return used++;
        }
    };

    // Keep adjacent submissions in one transient page; discard every range when
    // bgfx advances its frame and recycles the underlying storage.
    class TransientRanges {
        uint32_t capacity = 0;
        uint32_t used = 0;

    public:
        void reset() { capacity = used = 0; }

        template<class Reserve>
        std::optional<uint32_t> allocate(uint32_t count, Reserve reserve) {
            if (capacity - used < count) {
                const uint32_t available = reserve(std::max(count, 4096u));
                capacity = available >= count ? available : 0;
                used = 0;
            }
            if (capacity < count) {
                return std::nullopt;
            }
            const uint32_t first = used;
            used += count;
            return first;
        }
    };

    template<class T>
    class FrameKeepAlive {
        // bgfx uniform references must survive up to two frame calls with threaded rendering.
        std::array<std::vector<std::shared_ptr<const T>>, 3> frames;
        uint64_t frame = 0;

    public:
        uint64_t current_frame() const { return frame; }
        void keep(const std::shared_ptr<const T>& value) { frames[frame % frames.size()].push_back(value); }
        void advance() {
            ++frame;
            frames[frame % frames.size()].clear();
        }
        void clear() {
            for (auto& entries : frames) {
                entries.clear();
            }
            ++frame;
        }
    };

} // namespace

namespace {

    // raylib enum values the game passes through gfx.h
    constexpr int FILTER_POINT = 0, FILTER_BILINEAR = 1;
    constexpr int WRAP_REPEAT = 0, WRAP_CLAMP = 1, WRAP_MIRROR = 2;
    constexpr int BLEND_ALPHA = 0, BLEND_MULTIPLIED = 2, BLEND_ADD_COLORS = 3, BLEND_SUBTRACT_COLORS = 4,
                  BLEND_ALPHA_PREMULTIPLY = 5;
    constexpr int TEXT_LINE_SPACING = 2; // raylib's default, matches measure_text_ex

    struct Vertex {
        float x, y, z, u, v;
        uint32_t rgba; // bytes r,g,b,a in memory
    };

    struct TextureSlot {
        bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
        int filter = FILTER_POINT; // raylib's defaults
        int wrap = WRAP_REPEAT;
    };

    struct ProgramSlot {
        bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
        bgfx::ProgramHandle skinned = BGFX_INVALID_HANDLE;
        std::vector<std::string> names; // index = shader "location"
        std::vector<bgfx::UniformHandle> uniforms;
        std::vector<std::array<float, 4>> values;
        bool sprite_color = false;
        bool color_dirty = true;
        std::array<float, 4> color_transform{0, 1, 1, 0};
    };

    struct TargetSlot {
        bgfx::FrameBufferHandle fb = BGFX_INVALID_HANDLE;
        unsigned color = 0; // texture id
        int width = 0, height = 0;
    };

    struct Pass {
        bgfx::ViewId view = 0;
        unsigned target = 0; // render target id, 0 = screen
        int width = 0, height = 0;
        bool flip = false; // RT stored bottom-up like GL so the game's negative-height sampling holds
        bool clear = false;
        unsigned submits = 0;
    };

    struct Scissor {
        bool on = false;
        uint16_t x = 0, y = 0, w = 0, h = 0;
        bool operator==(const Scissor&) const = default;
    };

    SDL_Window* window = nullptr;
    unsigned window_flags = 0;
    bool should_close = false;
    uint32_t reset_flags = BGFX_RESET_NONE;
    const bgfx::Caps* caps = nullptr;

    bgfx::VertexLayout layout;
    bgfx::UniformHandle s_tex_color = BGFX_INVALID_HANDLE;
    unsigned white_texture = 0;
    Font default_font;

    // id = index + 1, so id 0 stays "none"
    std::vector<TextureSlot> textures;
    std::vector<ProgramSlot> programs; // [0] = default 2D program
    std::vector<TargetSlot> targets;
    ProgramSlot sprite_program;
    bool gpu_sprites_enabled = false;
    SpriteTextures sprite_textures;
    std::array<bgfx::UniformHandle, SpriteTextures::capacity> sprite_samplers = [] {
        std::array<bgfx::UniformHandle, SpriteTextures::capacity> handles;
        for (auto& handle : handles) {
            handle = BGFX_INVALID_HANDLE;
        }
        return handles;
    }();
    bgfx::VertexBufferHandle sprite_quad = BGFX_INVALID_HANDLE;
    std::vector<SpriteInstance> sprite_instances;
    bgfx::InstanceDataBuffer instance_page{};
    TransientRanges instance_ranges;
    FrameKeepAlive<CachedSkinPose> palette_frames;

    // frame / pass state
    bool frame_open = false;
    bgfx::ViewId next_view = 0;
    Pass pass;

    // Texture data queued for the next bgfx::frame(). bgfx uploads it all inside that call,
    // so a screen that loads hundreds of textures would hitch on its first frame.
    size_t pending_upload_bytes = 0;
    constexpr size_t UPLOAD_FLUSH_BYTES = 32u << 20;
    unsigned submits_this_frame = 0;
    bool screen_drawn = false; // a screen view this frame has a clear or a draw

    bgfx::TransientVertexBuffer vertex_page{};
    TransientRanges vertex_ranges;
    uint32_t transform_cache = UINT32_MAX;
    std::array<float, 16> transform_value{};

    uint32_t advance_frame(uint8_t flags = BGFX_FRAME_NONE) {
        const auto frame = bgfx::frame(flags);
        palette_frames.advance();
        vertex_ranges.reset();
        instance_ranges.reset();
        transform_cache = UINT32_MAX;
        return frame;
    }

    void flush_uploads_if_idle() {
        // Screens load in update(), after begin_drawing() but before any draw. While nothing is
        // submitted yet, FLUSH runs the queued uploads (and any pending clears) without presenting,
        // so the cost stays in the loading frame, as with raylib's glTexImage2D.
        if (pending_upload_bytes >= UPLOAD_FLUSH_BYTES && submits_this_frame == 0) {
            advance_frame(BGFX_FRAME_FLUSH);
            pending_upload_bytes = 0;
        }
    }

    // batch state
    std::vector<Vertex> verts;
    unsigned cur_texture = 0;
    unsigned cur_program = 0; // index into programs
    bool cur_lines = false;
    bool cur_sprites = false;
    uint64_t cur_blend = 0;
    uint64_t custom_separate_blend = 0;
    Scissor cur_scissor;
    std::array<float, 16> cur_model{};
    bool in_3d = false;

    constexpr std::array<float, 16> identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    bool sprite_compatible(unsigned program) {
        return program == 0 || programs[program].sprite_color;
    }

    std::array<float, 4> current_color_transform() {
        auto& program = programs[cur_program];
        if (program.sprite_color && program.color_dirty) {
            std::array<float, 4> source{}, target{};
            for (size_t i = 0; i < program.names.size(); ++i) {
                if (program.names[i] == "sourceColor") {
                    source = program.values[i];
                } else if (program.names[i] == "targetColor") {
                    target = program.values[i];
                }
            }
            program.color_transform = sprite_color_transform(source, target);
            program.color_dirty = false;
        }
        return program.color_transform;
    }

    // Reuses the first dead slot (per `alive`) or appends; returns index + 1.
    template<class T, class Alive>
    unsigned add_slot(std::vector<T>& slots, const T& slot, Alive alive) {
        for (size_t i = 0; i < slots.size(); i++) {
            if (!alive(slots[i])) {
                slots[i] = slot;
                return static_cast<unsigned>(i) + 1;
            }
        }
        slots.push_back(slot);
        return static_cast<unsigned>(slots.size());
    }
    unsigned add_slot(const TextureSlot& t) {
        return add_slot(textures, t, [](const TextureSlot& s) { return bgfx::isValid(s.handle); });
    }
    unsigned add_slot(const TargetSlot& t) {
        return add_slot(targets, t, [](const TargetSlot& s) { return bgfx::isValid(s.fb); });
    }
    unsigned add_program(const ProgramSlot& p) { // index; [0] is the default program
        return add_slot(programs, p, [](const ProgramSlot& s) { return bgfx::isValid(s.program); }) - 1;
    }

    void window_pixels(int& w, int& h) {
        SDL_GetWindowSizeInPixels(window, &w, &h);
    }

    TextureSlot* texture_slot(unsigned id) {
        return id && id <= textures.size() && bgfx::isValid(textures[id - 1].handle) ? &textures[id - 1] : nullptr;
    }

    uint64_t blend_state(int mode) {
        switch (mode) {
        case blend_mode::ADDITIVE:
            return BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
        case BLEND_MULTIPLIED:
            return BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_DST_COLOR, BGFX_STATE_BLEND_INV_SRC_ALPHA);
        case BLEND_ADD_COLORS:
            return BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE);
        case BLEND_SUBTRACT_COLORS:
            return BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE) |
                   BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_SUB);
        case BLEND_ALPHA_PREMULTIPLY:
            return BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
        case blend_mode::CUSTOM_SEPARATE:
            return custom_separate_blend;
        default:
            return BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
        }
    }

    uint32_t sampler_flags(const TextureSlot& t) {
        uint32_t f = 0;
        if (t.filter == FILTER_POINT) {
            f |= BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT;
        } else if (t.filter == FILTER_BILINEAR) {
            f |= BGFX_SAMPLER_MIP_POINT;
        } // trilinear and anisotropic: bgfx's default linear/linear/linear
        if (t.wrap == WRAP_CLAMP) {
            f |= BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
        } else if (t.wrap == WRAP_MIRROR) {
            f |= BGFX_SAMPLER_U_MIRROR | BGFX_SAMPLER_V_MIRROR;
        }
        return f;
    }

    void flush() {
        if (verts.empty() && sprite_instances.empty()) {
            return;
        }
        if (cur_sprites) {
            const auto count = static_cast<uint32_t>(sprite_instances.size());
            const auto first = instance_ranges.allocate(count, [&](uint32_t requested) {
                const uint32_t capacity = std::max(count, std::min(requested, 256u));
                const uint32_t available = bgfx::getAvailInstanceDataBuffer(capacity, sizeof(SpriteInstance));
                if (available >= count) {
                    bgfx::allocInstanceDataBuffer(&instance_page, available, sizeof(SpriteInstance));
                }
                return available;
            });
            if (!first) {
                sprite_instances.clear();
                sprite_textures.clear();
                return;
            }
            std::memcpy(instance_page.data + *first * sizeof(SpriteInstance), sprite_instances.data(),
                        count * sizeof(SpriteInstance));
            bgfx::setVertexBuffer(0, sprite_quad);
            bgfx::setInstanceDataBuffer(&instance_page, *first, count);
        } else {
            const auto n = static_cast<uint32_t>(verts.size());
            const auto first = vertex_ranges.allocate(n, [&](uint32_t requested) {
                const uint32_t capacity = bgfx::getAvailTransientVertexBuffer(requested, layout);
                if (capacity >= n) {
                    bgfx::allocTransientVertexBuffer(&vertex_page, capacity, layout);
                }
                return capacity;
            });
            if (!first) {
                static bool warned = false;
                if (!warned) {
                    spdlog::warn("bgfx: transient vertex buffer full, dropping draws");
                    warned = true;
                }
                verts.clear();
                return;
            }
            std::memcpy(vertex_page.data + *first * sizeof(Vertex), verts.data(), n * sizeof(Vertex));
            bgfx::setVertexBuffer(0, &vertex_page, *first, n);
        }
        if (transform_cache == UINT32_MAX || transform_value != cur_model) {
            transform_value = cur_model;
            transform_cache = bgfx::setTransform(cur_model.data());
        } else {
            bgfx::setTransform(transform_cache);
        }
        if (cur_sprites) {
            for (unsigned stage = 0; stage < SpriteTextures::capacity; ++stage) {
                const auto id = stage < sprite_textures.size() ? sprite_textures[stage] : white_texture;
                const TextureSlot* texture = texture_slot(id);
                if (!texture) {
                    texture = texture_slot(white_texture);
                }
                bgfx::setTexture(static_cast<uint8_t>(stage), sprite_samplers[stage], texture->handle,
                                 sampler_flags(*texture));
            }
        } else {
            const TextureSlot* tex = texture_slot(cur_texture);
            if (!tex) {
                tex = texture_slot(white_texture);
            }
            bgfx::setTexture(0, s_tex_color, tex->handle, sampler_flags(*tex));
        }
        if (cur_scissor.on) {
            bgfx::setScissor(cur_scissor.x, cur_scissor.y, cur_scissor.w, cur_scissor.h);
        }
        ProgramSlot& prog = cur_sprites ? sprite_program : programs[cur_program];
        for (size_t i = 0; i < prog.uniforms.size(); i++) {
            bgfx::setUniform(prog.uniforms[i], prog.values[i].data());
        }
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | cur_blend | (cur_lines ? BGFX_STATE_PT_LINES : 0));
        bgfx::submit(pass.view, prog.program);
        submits_this_frame++;
        pass.submits++;
        screen_drawn |= pass.target == 0;
        verts.clear();
        sprite_instances.clear();
        sprite_textures.clear();
    }

    // Starts a new view on `target` (0 = screen). Draws before it stay in earlier views.
    void open_pass(unsigned target, bool clear = false, Color clear_color = {}) {
        flush();
        // A view still empty (no draw, no clear) is reconfigured instead of spending a new id.
        const bool reuse = frame_open && pass.submits == 0 && !pass.clear;
        if (!frame_open) {
            next_view = 0;
            frame_open = true;
        }
        if (reuse) {
        } else if (next_view >= caps->limits.maxViews - 1) {
            static bool warned = false;
            if (!warned) {
                spdlog::warn("bgfx: out of views this frame; later passes share the last one");
                warned = true;
            }
        } else {
            pass.view = next_view++;
        }
        pass.target = target;
        pass.clear = clear;
        pass.submits = 0;
        if (const TargetSlot* t = target ? &targets[target - 1] : nullptr) {
            pass.width = t->width;
            pass.height = t->height;
            pass.flip = !caps->originBottomLeft;
        } else {
            window_pixels(pass.width, pass.height);
            pass.flip = false;
        }
        bgfx::setViewMode(pass.view, bgfx::ViewMode::Sequential);
        bgfx::setViewFrameBuffer(pass.view,
                                 target ? targets[target - 1].fb : bgfx::FrameBufferHandle BGFX_INVALID_HANDLE);
        bgfx::setViewRect(pass.view, 0, 0, static_cast<uint16_t>(pass.width), static_cast<uint16_t>(pass.height));
        bgfx::setViewClear(pass.view, clear ? BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH : BGFX_CLEAR_NONE,
                           static_cast<uint32_t>(clear_color.r) << 24 | static_cast<uint32_t>(clear_color.g) << 16 |
                               static_cast<uint32_t>(clear_color.b) << 8 | clear_color.a,
                           1.0f, 0);
        float proj[16];
        const float bottom = pass.flip ? 0.0f : static_cast<float>(pass.height);
        const float top = pass.flip ? static_cast<float>(pass.height) : 0.0f;
        bx::mtxOrtho(proj, 0.0f, static_cast<float>(pass.width), bottom, top, -1.0f, 1.0f, 0.0f,
                     caps->homogeneousDepth);
        bgfx::setViewTransform(pass.view, nullptr, proj);
        if (clear) {
            bgfx::touch(pass.view); // so the clear happens even without draws
            screen_drawn |= target == 0;
        }
        cur_scissor = {};
    }

    void use(unsigned texture, bool lines, bool sprites = false) {
        if ((!sprites && texture != cur_texture) || lines != cur_lines || sprites != cur_sprites) {
            flush();
            cur_texture = texture;
            cur_lines = lines;
            cur_sprites = sprites;
        }
    }

    bool batch_uses_texture(unsigned texture) {
        return cur_sprites ? sprite_textures.contains(texture) : cur_texture == texture;
    }

    uint32_t pack(Color c) {
        return static_cast<uint32_t>(c.r) | static_cast<uint32_t>(c.g) << 8 | static_cast<uint32_t>(c.b) << 16 |
               static_cast<uint32_t>(c.a) << 24;
    }

    void push_quad(Vec2 tl, Vec2 bl, Vec2 br, Vec2 tr, Vec2 uv_tl, Vec2 uv_bl, Vec2 uv_br, Vec2 uv_tr, Color c) {
        const uint32_t col = pack(c);
        const Vertex a{tl.x, tl.y, 0, uv_tl.x, uv_tl.y, col}, b{bl.x, bl.y, 0, uv_bl.x, uv_bl.y, col},
            d{br.x, br.y, 0, uv_br.x, uv_br.y, col}, e{tr.x, tr.y, 0, uv_tr.x, uv_tr.y, col};
        verts.insert(verts.end(), {a, b, d, a, d, e});
    }

    void push_triangle(Vec2 a, Vec2 b, Vec2 c, Color color) {
        const uint32_t col = pack(color);
        verts.insert(verts.end(),
                     {Vertex{a.x, a.y, 0, 0, 0, col}, Vertex{b.x, b.y, 0, 0, 0, col}, Vertex{c.x, c.y, 0, 0, 0, col}});
    }

    void push_line(Vec2 a, Vec2 b, Color c) {
        use(white_texture, true);
        const uint32_t col = pack(c);
        verts.insert(verts.end(), {Vertex{a.x, a.y, 0, 0, 0, col}, Vertex{b.x, b.y, 0, 0, 0, col}});
    }

    void fill_rect(Rect r, Color c) {
        if (sprite_compatible(cur_program) && !in_3d && gpu_sprites_enabled) {
            if (r.width < 0) {
                r.x += r.width;
                r.width = -r.width;
            }
            if (r.height < 0) {
                r.y += r.height;
                r.height = -r.height;
            }
            Texture white;
            white.id = white_texture;
            white.width = white.height = 1;
            draw_texture_pro(white, {0, 0, 1, 1}, r, {}, 0, c);
            return;
        }
        use(white_texture, false);
        push_quad({r.x, r.y}, {r.x, r.y + r.height}, {r.x + r.width, r.y + r.height}, {r.x + r.width, r.y}, {0, 0},
                  {0, 1}, {1, 1}, {1, 0}, c);
    }

    // raylib's automatic segment count for a quarter circle of `radius`
    int corner_segments(int segments, float radius) {
        if (segments >= 1) {
            return segments;
        }
        const float th = acosf(2 * powf(1 - 0.5f / radius, 2) - 1);
        const int s = static_cast<int>(ceilf((2 * std::numbers::pi_v<float> / th) / 4.0f));
        return s > 0 ? s : 4;
    }

    // Annulus sector (inner may be 0) around `center` from `start_deg`, 90 degrees.
    void corner(Vec2 center, float inner, float outer, float start_deg, int segments, Color c) {
        const float step = 90.0f / static_cast<float>(segments);
        for (int i = 0; i < segments; i++) {
            const float a0 = (start_deg + step * i) * deg2rad, a1 = (start_deg + step * (i + 1)) * deg2rad;
            const Vec2 o0{center.x + cosf(a0) * outer, center.y + sinf(a0) * outer};
            const Vec2 o1{center.x + cosf(a1) * outer, center.y + sinf(a1) * outer};
            const Vec2 i0{center.x + cosf(a0) * inner, center.y + sinf(a0) * inner};
            const Vec2 i1{center.x + cosf(a1) * inner, center.y + sinf(a1) * inner};
            push_triangle(i0, o1, o0, c);
            if (inner > 0.0f) {
                push_triangle(i0, i1, o1, c);
            }
        }
    }

    unsigned upload_texture(Image image, bool mipmaps) {
        Image rgba = image;
        if (image.format != pixel_format::RGBA8) {
            rgba = image_copy(image);
            image_format(&rgba, pixel_format::RGBA8);
        }
        int levels = 1;
        if (mipmaps) {
            for (int w = rgba.width, h = rgba.height; w > 1 || h > 1; w = std::max(1, w / 2), h = std::max(1, h / 2)) {
                levels++;
            }
        }
        TextureSlot slot;
        slot.handle = bgfx::createTexture2D(static_cast<uint16_t>(rgba.width), static_cast<uint16_t>(rgba.height),
                                            levels > 1, 1, bgfx::TextureFormat::RGBA8);
        // ponytail: box-filtered mips on the CPU; GPU generation if load time ever matters
        const auto* level = static_cast<const uint8_t*>(rgba.data);
        std::vector<uint8_t> next, prev;
        int w = rgba.width, h = rgba.height;
        for (int mip = 0; mip < levels; mip++) {
            const auto bytes = static_cast<uint32_t>(w * h * 4);
            bgfx::updateTexture2D(slot.handle, 0, static_cast<uint8_t>(mip), 0, 0, static_cast<uint16_t>(w),
                                  static_cast<uint16_t>(h), bgfx::copy(level, bytes));
            pending_upload_bytes += bytes;
            if (mip + 1 == levels) {
                break;
            }
            const int nw = std::max(1, w / 2), nh = std::max(1, h / 2);
            next.resize(static_cast<size_t>(nw) * nh * 4);
            for (int y = 0; y < nh; y++) {
                const int y0 = std::min(y * 2, h - 1), y1 = std::min(y * 2 + 1, h - 1);
                for (int x = 0; x < nw; x++) {
                    const int x0 = std::min(x * 2, w - 1), x1 = std::min(x * 2 + 1, w - 1);
                    for (int c = 0; c < 4; c++) {
                        next[(y * nw + x) * 4 + c] =
                            static_cast<uint8_t>((level[(y0 * w + x0) * 4 + c] + level[(y0 * w + x1) * 4 + c] +
                                                  level[(y1 * w + x0) * 4 + c] + level[(y1 * w + x1) * 4 + c] + 2) /
                                                 4);
                    }
                }
            }
            prev.swap(next);
            level = prev.data();
            w = nw;
            h = nh;
        }
        if (rgba.data != image.data) {
            unload_image(rgba);
        }
        flush_uploads_if_idle();
        return add_slot(slot);
    }

    ProgramSlot make_program(const uint8_t* vs, uint32_t vs_size, const uint8_t* fs, uint32_t fs_size) {
        ProgramSlot p;
        p.program = bgfx::createProgram(bgfx::createShader(bgfx::makeRef(vs, vs_size)),
                                        bgfx::createShader(bgfx::makeRef(fs, fs_size)), true);
        return p;
    }

    ProgramSlot make_mesh_program(const uint8_t* vs, uint32_t vs_size, const uint8_t* skin_vs, uint32_t skin_vs_size,
                                  const uint8_t* fs, uint32_t fs_size) {
        auto program = make_program(vs, vs_size, fs, fs_size);
        program.skinned = make_program(skin_vs, skin_vs_size, fs, fs_size).program;
        return program;
    }

    void native_handles(void*& nwh, void*& ndt, bgfx::NativeWindowHandleType::Enum& type) {
#ifdef YATAIDON_USE_SDL2 // no window properties: the platform's renderer bridge takes the SDL window
        nwh = window;
        ndt = nullptr;
        type = bgfx::NativeWindowHandleType::Default;
#else
        SDL_PropertiesID props = SDL_GetWindowProperties(window);
#ifdef __ANDROID__
        ndt = nullptr;
        nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);
        type = bgfx::NativeWindowHandleType::Default;
        return;
#endif
        if (std::strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
            ndt = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
            nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
            type = bgfx::NativeWindowHandleType::Wayland;
        } else {
            ndt = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
            nwh = reinterpret_cast<void*>(
                static_cast<uintptr_t>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)));
            type = bgfx::NativeWindowHandleType::Default;
        }
#endif
    }

    bgfx::RendererType::Enum renderer_from_env() {
        const char* name = std::getenv("YATAIDON_BGFX_RENDERER");
        if (!name) {
            return bgfx::RendererType::Vulkan; // only SPIR-V shaders are built so far
        }
        for (int i = 0; i < bgfx::RendererType::Count; i++) {
            if (SDL_strcasecmp(name, bgfx::getRendererName(static_cast<bgfx::RendererType::Enum>(i))) == 0) {
                return static_cast<bgfx::RendererType::Enum>(i);
            }
        }
        spdlog::warn("bgfx: unknown YATAIDON_BGFX_RENDERER '{}', using Vulkan", name);
        return bgfx::RendererType::Vulkan;
    }

    void reset_backbuffer() {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        bgfx::SwapChain sc;
        bgfx::NativeWindowHandleType::Enum type;
        native_handles(sc.nwh, sc.ndt, type);
        sc.width = static_cast<uint32_t>(w);
        sc.height = static_cast<uint32_t>(h);
        bgfx::reset(reset_flags, &sc);
    }

    // Logs bgfx errors and saves requested screenshots as PNG (RGBA, top row first).
    struct Callback : bgfx::CallbackI {
        void fatal(const char* file, uint16_t line, bgfx::Fatal::Enum, const char* str) override {
            spdlog::critical("bgfx fatal {}:{}: {}", file, line, str);
            std::abort();
        }
        void traceVargs(const char*, uint16_t, const char*, va_list) override {}
        void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
        void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
        void profilerEnd() override {}
        // Vulkan pipeline caches, so pipelines compile once per machine instead of on every run.
        static std::string cache_path(uint64_t id) { return "cache/bgfx/" + std::to_string(id) + ".bin"; }
        uint32_t cacheReadSize(uint64_t id) override {
            std::error_code ec;
            const auto size = std::filesystem::file_size(cache_path(id), ec);
            return ec ? 0 : static_cast<uint32_t>(size);
        }
        bool cacheRead(uint64_t id, void* data, uint32_t size) override {
            std::ifstream f(cache_path(id), std::ios::binary);
            return f.read(static_cast<char*>(data), size) && f.gcount() == size;
        }
        void cacheWrite(uint64_t id, const void* data, uint32_t size) override {
            std::error_code ec;
            std::filesystem::create_directories("cache/bgfx", ec);
            std::ofstream(cache_path(id), std::ios::binary).write(static_cast<const char*>(data), size);
        }
        void screenShot(const char* path, uint32_t w, uint32_t h, uint32_t pitch, bgfx::TextureFormat::Enum format,
                        const void* data, uint32_t, bool yflip) override {
            Image img = gen_image_color(static_cast<int>(w), static_cast<int>(h), Color::BLACK);
            auto* out = static_cast<uint8_t*>(img.data);
            const bool bgra = format == bgfx::TextureFormat::BGRA8;
            for (uint32_t y = 0; y < h; y++) {
                const auto* row = static_cast<const uint8_t*>(data) + (yflip ? h - 1 - y : y) * pitch;
                for (uint32_t x = 0; x < w; x++) {
                    uint8_t* o = out + (y * w + x) * 4;
                    o[0] = row[x * 4 + (bgra ? 2 : 0)];
                    o[1] = row[x * 4 + 1];
                    o[2] = row[x * 4 + (bgra ? 0 : 2)];
                    o[3] = 255;
                }
            }
            export_image(img, path);
            unload_image(img);
            spdlog::info("bgfx: saved {}", path);
        }
        void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
        void captureEnd() override {}
        void captureFrame(const void*, uint32_t) override {}
    } callback;

    // Debug aid: YATAIDON_BGFX_SHOT="3,8:/tmp/shot" saves /tmp/shot0.png at 3 s, /tmp/shot1.png at 8 s.
    std::vector<double> shot_times;
    std::string shot_prefix;
    size_t shot_index = 0;

    void parse_shot_env() {
        const char* env = std::getenv("YATAIDON_BGFX_SHOT");
        const char* colon = env ? std::strchr(env, ':') : nullptr;
        if (!colon) {
            return;
        }
        shot_prefix = colon + 1;
        for (const char* p = env; p < colon; p = std::strchr(p, ',') ? std::strchr(p, ',') + 1 : colon) {
            shot_times.push_back(std::strtod(p, nullptr));
        }
    }

    void maybe_request_shot() {
        if (shot_index < shot_times.size() && SDL_GetTicks() / 1000.0 >= shot_times[shot_index]) {
            const std::string path = shot_prefix + std::to_string(shot_index++) + ".png";
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
        }
    }

    // affine 2D camera as a bgfx (column-major) matrix, matching raylib's GetCameraMatrix2D
    std::array<float, 16> camera_matrix(const Camera2D& cam) {
        const float s = sinf(cam.rotation * deg2rad) * cam.zoom, c = cosf(cam.rotation * deg2rad) * cam.zoom;
        const float tx = cam.offset.x - (c * cam.target.x - s * cam.target.y);
        const float ty = cam.offset.y - (s * cam.target.x + c * cam.target.y);
        return {c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, tx, ty, 0, 1};
    }

    // defined with the 3D code at the end
    void init_3d();
    void shutdown_3d();

} // namespace

// ---- window ---------------------------------------------------------------------------

SDL_Window* main_window() {
    return window;
}

void set_window_flags(unsigned flags) {
    window_flags = flags;
}

void quiet_raylib_log() {}

void disable_exit_key() {}

void init_window(int width, int height, const char* title) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        spdlog::critical("SDL video init failed: {}", SDL_GetError());
        std::abort();
    }
#ifdef YATAIDON_USE_SDL2
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height,
                              (window_flags & window_flag::RESIZABLE) ? SDL_WINDOW_RESIZABLE : 0);
#else
    window = SDL_CreateWindow(title, width, height, (window_flags & window_flag::RESIZABLE) ? SDL_WINDOW_RESIZABLE : 0);
#endif
    if (!window) {
        spdlog::critical("SDL_CreateWindow failed: {}", SDL_GetError());
        std::abort();
    }
    if (!platform_threaded_renderer()) {
        bgfx::renderFrame();
    }

    bgfx::Init init;
    init.type = renderer_from_env();
    native_handles(init.swapChain.nwh, init.swapChain.ndt, init.platformData.type);
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    init.swapChain.width = static_cast<uint32_t>(w);
    init.swapChain.height = static_cast<uint32_t>(h);
    reset_flags = (window_flags & window_flag::VSYNC) ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
    init.reset = reset_flags;
    init.callback = &callback;
    parse_shot_env();
    if (!bgfx::init(init)) {
        spdlog::critical("bgfx init failed");
        std::abort();
    }
    caps = bgfx::getCaps();
    spdlog::info("bgfx: {} renderer, {}x{}", bgfx::getRendererName(bgfx::getRendererType()), w, h);
    if (const char* dbg = std::getenv("YATAIDON_BGFX_DEBUG")) {
        bgfx::setDebug(std::strtol(dbg, nullptr, 10) ? BGFX_DEBUG_STATS : BGFX_DEBUG_NONE);
    }

    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();
    s_tex_color = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    programs.push_back(make_program(vs_2d_spv, sizeof(vs_2d_spv), fs_2d_spv, sizeof(fs_2d_spv)));
    gpu_sprites_enabled = false;
    if (caps->limits.maxInstanceData >= 4 && caps->limits.maxVertexAttributes >= 5 &&
        caps->limits.maxTextureSamplers >= SpriteTextures::capacity) {
        sprite_instances.reserve(256);
        constexpr float corners[] = {0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0};
        bgfx::VertexLayout quad_layout;
        quad_layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
        sprite_quad = bgfx::createVertexBuffer(bgfx::copy(corners, sizeof(corners)), quad_layout);
        sprite_program = make_program(vs_sprite_spv, sizeof(vs_sprite_spv), fs_sprite_spv, sizeof(fs_sprite_spv));
        for (unsigned stage = 0; stage < SpriteTextures::capacity; ++stage) {
            const std::string name = "s_sprite" + std::to_string(stage);
            sprite_samplers[stage] = bgfx::createUniform(name.c_str(), bgfx::UniformType::Sampler);
        }
        gpu_sprites_enabled =
            bgfx::isValid(sprite_quad) && bgfx::isValid(sprite_program.program) &&
            std::ranges::all_of(sprite_samplers, [](const auto& sampler) { return bgfx::isValid(sampler); });
    }
    init_3d();

    Image white = gen_image_color(1, 1, Color::WHITE);
    white_texture = upload_texture(white, false);
    unload_image(white);

    default_font = load_font_from_memory(default_font_ttf(), 32);

    set_separate_alpha_blend();
    cur_blend = blend_state(BLEND_ALPHA);
    cur_model = identity;
}

void close_window() {
    gpu_sprites_enabled = false;
    verts.clear();
    sprite_instances.clear();
    sprite_textures.clear();
    instance_ranges.reset();
    cur_sprites = false;
    if (bgfx::isValid(sprite_quad)) {
        bgfx::destroy(sprite_quad);
        sprite_quad = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(sprite_program.program)) {
        bgfx::destroy(sprite_program.program);
        sprite_program = {};
    }
    for (auto& sampler : sprite_samplers) {
        if (bgfx::isValid(sampler)) {
            bgfx::destroy(sampler);
            sampler = BGFX_INVALID_HANDLE;
        }
    }
    for (auto& t : targets) {
        if (bgfx::isValid(t.fb)) {
            bgfx::destroy(t.fb);
        }
    }
    for (auto& t : textures) {
        if (bgfx::isValid(t.handle)) {
            bgfx::destroy(t.handle);
        }
    }
    for (auto& p : programs) {
        if (bgfx::isValid(p.skinned)) {
            bgfx::destroy(p.skinned);
        }
        if (bgfx::isValid(p.program)) {
            bgfx::destroy(p.program);
            for (auto u : p.uniforms) {
                bgfx::destroy(u);
            }
        }
    }
    shutdown_3d();
    targets.clear();
    textures.clear();
    programs.clear();
    bgfx::destroy(s_tex_color);
    bgfx::shutdown();
    palette_frames.clear();
    SDL_DestroyWindow(window);
    window = nullptr;
}

void poll_window_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) { // window events only; input reads SDL through event watches
        if (e.type == SDL_EVENT_QUIT) {
            should_close = true;
#ifdef YATAIDON_USE_SDL2
        } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
#else
        } else if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
#endif
            reset_backbuffer();
        }
    }
}

bool window_should_close() {
    return should_close;
}

void toggle_fullscreen() {
    SDL_SetWindowFullscreen(window, !is_window_fullscreen());
}

void toggle_borderless_windowed() {
    toggle_fullscreen(); // SDL3 fullscreen without a display mode is borderless desktop, as raylib's SDL path
}

bool is_window_fullscreen() {
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void set_window_size(int width, int height) {
    SDL_SetWindowSize(window, width, height);
}

int get_screen_width() {
    int w = 0;
    SDL_GetWindowSize(window, &w, nullptr);
    return w;
}

int get_screen_height() {
    int h = 0;
    SDL_GetWindowSize(window, nullptr, &h);
    return h;
}

int get_render_width() {
    if (pass.target) { // like raylib: the render target's size while one is bound
        return pass.width;
    }
    int w = 0, h = 0;
    window_pixels(w, h);
    return w;
}

int get_render_height() {
    if (pass.target) {
        return pass.height;
    }
    int w = 0, h = 0;
    window_pixels(w, h);
    return h;
}

int get_monitor_refresh_rate() {
#ifdef YATAIDON_USE_SDL2
    SDL_DisplayMode storage{};
    const SDL_DisplayMode* mode =
        SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &storage) == 0 ? &storage : nullptr;
#else
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
#endif
    return mode ? static_cast<int>(mode->refresh_rate) : 0;
}

// ---- frame / passes -------------------------------------------------------------------

void begin_drawing() {
    open_pass(0);
    cur_model = identity;
}

void end_drawing() {
    flush();
}

void swap_screen_buffer() {
    flush();
    if (!screen_drawn) { // the backbuffer still needs a pass to be presented
        if (pass.target != 0) {
            open_pass(0);
        }
        bgfx::touch(pass.view);
    }
    screen_drawn = false;
    pending_upload_bytes = 0; // uploaded by this frame
    submits_this_frame = 0;
    maybe_request_shot();
    advance_frame();
    frame_open = false;
    pass.target = 0;
}

void clear_background(Color color) {
    open_pass(pass.target, true, color);
}

RenderTarget load_render_texture(int width, int height) {
    if (width <= 0 || height <= 0 || width > caps->limits.maxTextureSize || height > caps->limits.maxTextureSize) {
        return {};
    }
    constexpr uint64_t depth_flags = BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY;
    bgfx::TextureFormat::Enum depth_format = bgfx::TextureFormat::Count;
    // Vulkan drivers need not support D24S8. Check before creation: bgfx may
    // otherwise substitute a colour format, which cannot serve as depth.
    for (auto format : {bgfx::TextureFormat::D24S8, bgfx::TextureFormat::D32F, bgfx::TextureFormat::D16}) {
        if (bgfx::isTextureValid(0, false, 1, format, depth_flags)) {
            depth_format = format;
            break;
        }
    }
    if (depth_format == bgfx::TextureFormat::Count ||
        !bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT)) {
        spdlog::warn("bgfx: no supported colour/depth render target format");
        return {};
    }
    TargetSlot t;
    t.width = width;
    t.height = height;
    TextureSlot color;
    color.handle = bgfx::createTexture2D(static_cast<uint16_t>(width), static_cast<uint16_t>(height), false, 1,
                                         bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT);
    bgfx::TextureHandle depth = bgfx::createTexture2D(static_cast<uint16_t>(width), static_cast<uint16_t>(height),
                                                      false, 1, depth_format, depth_flags);
    if (!bgfx::isValid(color.handle) || !bgfx::isValid(depth)) {
        if (bgfx::isValid(color.handle)) {
            bgfx::destroy(color.handle);
        }
        if (bgfx::isValid(depth)) {
            bgfx::destroy(depth);
        }
        return {};
    }
    bgfx::Attachment att[2];
    att[0].init(color.handle, bgfx::Access::Write, 0, 1, 0, BGFX_ATTACHMENT_NONE);
    att[1].init(depth, bgfx::Access::Write, 0, 1, 0, BGFX_ATTACHMENT_NONE);
    t.fb = bgfx::createFrameBuffer(2, att, false);
    bgfx::destroy(depth); // the framebuffer keeps it alive on success
    if (!bgfx::isValid(t.fb)) {
        bgfx::destroy(color.handle);
        spdlog::warn("bgfx: failed to create {}x{} model framebuffer", width, height);
        return {};
    }
    t.color = add_slot(color);

    RenderTarget rt;
    rt.id = add_slot(t);
    rt.texture.id = t.color;
    rt.texture.width = width;
    rt.texture.height = height;
    rt.texture.mipmaps = 1;
    rt.texture.format = pixel_format::RGBA8;
    return rt;
}

void unload_render_texture(RenderTarget target) {
    if (!target.id || target.id > targets.size() || !bgfx::isValid(targets[target.id - 1].fb)) {
        return;
    }
    TargetSlot& t = targets[target.id - 1];
    bgfx::destroy(t.fb);
    t.fb = BGFX_INVALID_HANDLE;
    Texture color;
    color.id = t.color;
    unload_texture(color);
}

void begin_texture_mode(RenderTarget target) {
    open_pass(target.id);
    cur_model = identity;
}

void end_texture_mode() {
    open_pass(0);
    cur_model = identity;
}

// ---- state ----------------------------------------------------------------------------

void begin_scissor_mode(int x, int y, int width, int height) {
    if (pass.flip) {
        y = pass.height - (y + height);
    }
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    Scissor s{true, static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(std::max(width, 0)),
              static_cast<uint16_t>(std::max(height, 0))};
    if (!(s == cur_scissor)) {
        flush();
        cur_scissor = s;
    }
}

void end_scissor_mode() {
    if (cur_scissor.on) {
        flush();
        cur_scissor = {};
    }
}

void begin_blend_mode(int mode) {
    const uint64_t b = blend_state(mode);
    if (b != cur_blend) {
        flush();
        cur_blend = b;
    }
}

void end_blend_mode() {
    begin_blend_mode(BLEND_ALPHA);
}

void set_separate_alpha_blend() {
    custom_separate_blend = BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA,
                                                           BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
}

void begin_mode_2d(Camera2D camera) {
    flush();
    cur_model = camera_matrix(camera);
}

void end_mode_2d() {
    flush();
    cur_model = identity;
}

Vec2 get_world_to_screen_2d(Vec2 p, Camera2D camera) {
    const auto m = camera_matrix(camera);
    return {m[0] * p.x + m[4] * p.y + m[12], m[1] * p.x + m[5] * p.y + m[13]};
}

Vec2 get_screen_to_world_2d(Vec2 p, Camera2D camera) {
    const auto m = camera_matrix(camera);
    const float det = m[0] * m[5] - m[4] * m[1];
    if (det == 0.0f) {
        return {};
    }
    const float x = p.x - m[12], y = p.y - m[13];
    return {(m[5] * x - m[4] * y) / det, (m[0] * y - m[1] * x) / det};
}

// ---- textures -------------------------------------------------------------------------

Texture load_texture_from_image(Image image, bool mipmaps) {
    Texture tex;
    if (!image.data || image.width <= 0 || image.height <= 0) {
        return tex;
    }
    tex.id = upload_texture(image, mipmaps);
    tex.width = image.width;
    tex.height = image.height;
    tex.mipmaps = 1;
    tex.format = image.format;
    if (mipmaps) {
        for (int w = image.width, h = image.height; w > 1 || h > 1; w /= 2, h /= 2) {
            tex.mipmaps++;
        }
    }
    return tex;
}

void unload_texture(Texture tex) {
    if (TextureSlot* t = texture_slot(tex.id)) {
        if (batch_uses_texture(tex.id)) {
            flush();
        }
        bgfx::destroy(t->handle);
        t->handle = BGFX_INVALID_HANDLE;
    }
}

bool is_texture_valid(Texture tex) {
    return texture_slot(tex.id) != nullptr;
}

void set_texture_filter(Texture tex, int filter) {
    if (TextureSlot* t = texture_slot(tex.id)) {
        if (t->filter == filter) {
            return;
        }
        if (batch_uses_texture(tex.id)) {
            flush();
        }
        t->filter = filter;
    }
}

void set_texture_wrap(Texture tex, int wrap) {
    if (TextureSlot* t = texture_slot(tex.id)) {
        if (t->wrap == wrap) {
            return;
        }
        if (batch_uses_texture(tex.id)) {
            flush();
        }
        t->wrap = wrap;
    }
}

void update_texture(Texture tex, const void* pixels) {
    TextureSlot* t = texture_slot(tex.id);
    if (!t || !pixels) {
        return;
    }
    if (batch_uses_texture(tex.id)) {
        flush();
    }
    Image src{const_cast<void*>(pixels), tex.width, tex.height, 1, tex.format};
    Image rgba = src;
    if (tex.format != pixel_format::RGBA8) {
        rgba = image_copy(src);
        image_format(&rgba, pixel_format::RGBA8);
    }
    bgfx::updateTexture2D(t->handle, 0, 0, 0, 0, static_cast<uint16_t>(tex.width), static_cast<uint16_t>(tex.height),
                          bgfx::copy(rgba.data, static_cast<uint32_t>(tex.width * tex.height * 4)));
    if (rgba.data != pixels) {
        unload_image(rgba);
    }
}

Image load_image_from_texture(Texture tex) {
    const TextureSlot* t = texture_slot(tex.id);
    if (!t) {
        return {};
    }
    flush();
    // Blit mip 0 into a CPU-readable copy and run frames until it arrives. Those are FLUSH frames
    // (nothing presented); the game reads textures back while loading (Chara3D recolors).
    bgfx::TextureHandle staging =
        bgfx::createTexture2D(static_cast<uint16_t>(tex.width), static_cast<uint16_t>(tex.height), false, 1,
                              bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
    bgfx::TextureRegion dst, src;
    dst.init(staging);
    src.init(t->handle);
    bgfx::blit(pass.view, dst, src);
    Image img = gen_image_color(tex.width, tex.height, Color::BLANK);
    const uint32_t ready = bgfx::read(dst, img.data);
    for (int i = 0; i < 8 && advance_frame(BGFX_FRAME_FLUSH) < ready; i++) {
    }
    pending_upload_bytes = 0;
    bgfx::destroy(staging);
    return img;
}

Image load_image_from_screen() {
    // ponytail: needs bgfx::requestScreenShot + a CallbackI; F12 screenshots are disabled until then
    spdlog::warn("bgfx: screenshots not implemented yet");
    return {};
}

// ---- shaders --------------------------------------------------------------------------

Shader load_shader(const char* vs_path, const char* fs_path) {
    // The game's GLSL files, by fragment stage, as their shader/bgfx ports. The vertex stage
    // follows from it: 2D (dummy.vs / pass.vs), mesh (raylib's default) or the outline hull.
    struct Port {
        const char* fs;
        const uint8_t *vs_bin, *fs_bin;
        uint32_t vs_size, fs_size;
        const uint8_t* skin_vs = nullptr;
        uint32_t skin_vs_size = 0;
    };
#define PORT(name, vs, fs)                                                                                             \
    {                                                                                                                  \
        name, vs##_spv, fs##_spv, sizeof(vs##_spv), sizeof(fs##_spv)                                                   \
    }
    static const Port ports[] = {
        PORT("colortransform.fs", vs_2d, fs_colortransform),
        PORT("outline_fxaa.fs", vs_2d, fs_outline_fxaa),
        {"cutout.fs", vs_mesh_spv, fs_cutout_spv, sizeof(vs_mesh_spv), sizeof(fs_cutout_spv), vs_mesh_skin_spv,
         sizeof(vs_mesh_skin_spv)},
        {"face.fs", vs_mesh_spv, fs_face_spv, sizeof(vs_mesh_spv), sizeof(fs_face_spv), vs_mesh_skin_spv,
         sizeof(vs_mesh_skin_spv)},
        {"null.fs", vs_mesh_spv, fs_null_spv, sizeof(vs_mesh_spv), sizeof(fs_null_spv), vs_mesh_skin_spv,
         sizeof(vs_mesh_skin_spv)},
        {"outline.fs", vs_outline_spv, fs_outline_spv, sizeof(vs_outline_spv), sizeof(fs_outline_spv),
         vs_outline_skin_spv, sizeof(vs_outline_skin_spv)},
    };
#undef PORT
    const std::string fs = fs_path ? fs_path : "";
    Shader shader;
    for (const Port& p : ports) {
        if (fs.ends_with(std::string("/") + p.fs)) {
            shader.id = add_program(
                p.skin_vs ? make_mesh_program(p.vs_bin, p.vs_size, p.skin_vs, p.skin_vs_size, p.fs_bin, p.fs_size)
                          : make_program(p.vs_bin, p.vs_size, p.fs_bin, p.fs_size));
            programs[shader.id].sprite_color = fs.ends_with("/colortransform.fs");
            return shader;
        }
    }
    spdlog::warn("bgfx: shader '{}' + '{}' not ported", vs_path ? vs_path : "default", fs);
    return shader;
}

void unload_shader(Shader shader) {
    if (!is_shader_valid(shader)) {
        return;
    }
    if (cur_program == shader.id) {
        flush();
        cur_program = 0;
    }
    ProgramSlot& p = programs[shader.id];
    bgfx::destroy(p.program);
    if (bgfx::isValid(p.skinned)) {
        bgfx::destroy(p.skinned);
    }
    for (auto u : p.uniforms) {
        bgfx::destroy(u);
    }
    p = ProgramSlot{};
}

bool is_shader_valid(Shader shader) {
    return shader.id && shader.id < programs.size() && bgfx::isValid(programs[shader.id].program);
}

int get_shader_location(Shader shader, const char* name) {
    if (!is_shader_valid(shader)) {
        return -1;
    }
    ProgramSlot& p = programs[shader.id];
    for (size_t i = 0; i < p.names.size(); i++) {
        if (p.names[i] == name) {
            return static_cast<int>(i);
        }
    }
    p.names.emplace_back(name);
    p.uniforms.push_back(bgfx::createUniform(name, bgfx::UniformType::Vec4));
    p.values.push_back({});
    return static_cast<int>(p.names.size()) - 1;
}

void set_shader_value(Shader shader, int loc, const void* value, int uniform_type) {
    if (!is_shader_valid(shader) || loc < 0 || static_cast<size_t>(loc) >= programs[shader.id].values.size()) {
        return;
    }
    const int n = uniform_type == shader_uniform::VEC3 ? 3 : uniform_type == shader_uniform::VEC2 ? 2 : 1;
    auto& v = programs[shader.id].values[loc];
    std::array<float, 4> updated{};
    std::memcpy(updated.data(), value, n * sizeof(float));
    if (std::memcmp(v.data(), updated.data(), sizeof(updated)) == 0) {
        return;
    }
    if (cur_program == shader.id && !(cur_sprites && gpu_sprites_enabled && programs[shader.id].sprite_color)) {
        flush(); // Submit queued vertices with the previous uniform values.
    }
    v = updated;
    programs[shader.id].color_dirty = true;
}

void begin_shader_mode(Shader shader) {
    const unsigned prog = is_shader_valid(shader) ? shader.id : 0;
    if (prog != cur_program) {
        if (!gpu_sprites_enabled || !sprite_compatible(prog) || !sprite_compatible(cur_program) || !verts.empty()) {
            flush();
        }
        cur_program = prog;
    }
}

void end_shader_mode() {
    begin_shader_mode(Shader{});
}

// ---- drawing --------------------------------------------------------------------------

void draw_texture_pro(Texture tex, Rect src, Rect dst, Vec2 origin, float rotation, Color tint) {
    if (!texture_slot(tex.id)) {
        return;
    }
    if (sprite_compatible(cur_program) && tint.a == 0 &&
        (cur_blend == blend_state(BLEND_ALPHA) || cur_blend == custom_separate_blend)) {
        return;
    }
    const float w = static_cast<float>(tex.width), h = static_cast<float>(tex.height);
    bool flip_x = false;
    if (src.width < 0) {
        flip_x = true;
        src.width *= -1;
    }
    if (src.height < 0) {
        src.y -= src.height;
    }
    dst.width = std::abs(dst.width);
    dst.height = std::abs(dst.height);

    if (sprite_compatible(cur_program) && !in_3d && gpu_sprites_enabled) {
        if (rotation == 0.0f && cur_model[1] == 0.0f && cur_model[4] == 0.0f) {
            const float x0 = (dst.x - origin.x) * cur_model[0] + cur_model[12];
            const float x1 = (dst.x - origin.x + dst.width) * cur_model[0] + cur_model[12];
            const float y0 = (dst.y - origin.y) * cur_model[5] + cur_model[13];
            const float y1 = (dst.y - origin.y + dst.height) * cur_model[5] + cur_model[13];
            if (std::max(x0, x1) < -1.0f || std::min(x0, x1) > pass.width + 1.0f || std::max(y0, y1) < -1.0f ||
                std::min(y0, y1) > pass.height + 1.0f) {
                return;
            }
        }
        const float u0 = src.x / w, u1 = (src.x + src.width) / w;
        use(tex.id, false, true);
        const auto texture_index = sprite_textures.add(tex.id, [] { flush(); });
        static_assert(SpriteTextures::capacity <= 8);
        sprite_instances.push_back(
            SpriteInstance::make(dst, {flip_x ? u1 : u0, src.y / h, flip_x ? u0 : u1, (src.y + src.height) / h}, origin,
                                 rotation * deg2rad, tint, texture_index, current_color_transform()));
        return;
    }

    Vec2 tl, tr, bl, br;
    if (rotation == 0.0f) {
        const float x = dst.x - origin.x, y = dst.y - origin.y;
        tl = {x, y};
        tr = {x + dst.width, y};
        bl = {x, y + dst.height};
        br = {x + dst.width, y + dst.height};
    } else {
        const float s = sinf(rotation * deg2rad), c = cosf(rotation * deg2rad);
        const float dx = -origin.x, dy = -origin.y;
        tl = {dst.x + dx * c - dy * s, dst.y + dx * s + dy * c};
        tr = {dst.x + (dx + dst.width) * c - dy * s, dst.y + (dx + dst.width) * s + dy * c};
        bl = {dst.x + dx * c - (dy + dst.height) * s, dst.y + dx * s + (dy + dst.height) * c};
        br = {dst.x + (dx + dst.width) * c - (dy + dst.height) * s,
              dst.y + (dx + dst.width) * s + (dy + dst.height) * c};
    }
    // Both supported sprite programs leave vertex positions unchanged.
    if (sprite_compatible(cur_program) && !in_3d && cur_model[1] == 0.0f && cur_model[4] == 0.0f) {
        const float left = std::min({tl.x, tr.x, bl.x, br.x}) * cur_model[0] + cur_model[12];
        const float right = std::max({tl.x, tr.x, bl.x, br.x}) * cur_model[0] + cur_model[12];
        const float top = std::min({tl.y, tr.y, bl.y, br.y}) * cur_model[5] + cur_model[13];
        const float bottom = std::max({tl.y, tr.y, bl.y, br.y}) * cur_model[5] + cur_model[13];
        if (std::max(left, right) < -1.0f || std::min(left, right) > pass.width + 1.0f ||
            std::max(top, bottom) < -1.0f || std::min(top, bottom) > pass.height + 1.0f) {
            return;
        }
    }
    const float u0 = src.x / w, u1 = (src.x + src.width) / w;
    const float v0 = src.y / h, v1 = (src.y + src.height) / h;
    use(tex.id, false);
    if (flip_x) {
        push_quad(tl, bl, br, tr, {u1, v0}, {u1, v1}, {u0, v1}, {u0, v0}, tint);
    } else {
        push_quad(tl, bl, br, tr, {u0, v0}, {u0, v1}, {u1, v1}, {u1, v0}, tint);
    }
}

void draw_rectangle(Rect rec, Color color) {
    fill_rect(rec, color);
}

void draw_rectangle(int x, int y, int w, int h, Color color) {
    fill_rect({static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)}, color);
}

void draw_rectangle_lines_ex(Rect rec, float thick, Color color) {
    if (thick > rec.width || thick > rec.height) {
        thick = rec.width >= rec.height ? rec.height / 2 : rec.width / 2;
    }
    fill_rect({rec.x, rec.y, rec.width, thick}, color);
    fill_rect({rec.x, rec.y - thick + rec.height, rec.width, thick}, color);
    fill_rect({rec.x, rec.y + thick, thick, rec.height - thick * 2.0f}, color);
    fill_rect({rec.x - thick + rec.width, rec.y + thick, thick, rec.height - thick * 2.0f}, color);
}

void draw_rectangle_lines(int x, int y, int w, int h, Color color) {
    const float ox = 0.5f / cur_model[0], oy = 0.5f / cur_model[5]; // pixel centers, as raylib
    const float l = static_cast<float>(x) + ox, t = static_cast<float>(y) + oy, r = static_cast<float>(x + w) - ox,
                b = static_cast<float>(y + h) - oy;
    push_line({l, t}, {r, t}, color);
    push_line({r, t}, {r, b}, color);
    push_line({r, b}, {l, b}, color);
    push_line({l, b}, {l, t}, color);
}

void draw_line(int x0, int y0, int x1, int y1, Color color) {
    push_line({static_cast<float>(x0), static_cast<float>(y0)}, {static_cast<float>(x1), static_cast<float>(y1)},
              color);
}

void draw_rectangle_rounded(Rect rec, float roundness, int segments, Color color) {
    if (roundness <= 0.0f) {
        fill_rect(rec, color);
        return;
    }
    roundness = std::min(roundness, 1.0f);
    const float radius = (rec.width > rec.height ? rec.height : rec.width) * roundness / 2;
    if (radius <= 0.0f) {
        return;
    }
    segments = corner_segments(segments, radius);
    const float x0 = rec.x + radius, x1 = rec.x + rec.width - radius;
    const float y0 = rec.y + radius, y1 = rec.y + rec.height - radius;
    use(white_texture, false);
    corner({x0, y0}, 0, radius, 180, segments, color);
    corner({x1, y0}, 0, radius, 270, segments, color);
    corner({x1, y1}, 0, radius, 0, segments, color);
    corner({x0, y1}, 0, radius, 90, segments, color);
    fill_rect({x0, rec.y, x1 - x0, rec.height}, color);
    fill_rect({rec.x, y0, radius, y1 - y0}, color);
    fill_rect({x1, y0, radius, y1 - y0}, color);
}

void draw_rectangle_rounded_lines_ex(Rect rec, float roundness, int segments, float thick, Color color) {
    if (roundness <= 0.0f) {
        draw_rectangle_lines_ex(rec, thick, color);
        return;
    }
    roundness = std::min(roundness, 1.0f);
    const float radius = (rec.width > rec.height ? rec.height : rec.width) * roundness / 2;
    if (radius <= 0.0f) {
        return;
    }
    // ponytail: inward outlines only (thick >= 0), the only kind the game draws
    const float outer = radius;
    float inner = outer - thick, band = thick;
    if (inner <= 0.0f) {
        inner = 0.0f;
        band = outer;
        draw_rectangle_lines_ex({rec.x + outer, rec.y + outer, rec.width - outer * 2, rec.height - outer * 2},
                                thick - outer, color);
    }
    segments = corner_segments(segments, outer);
    const float x0 = rec.x + outer, x1 = rec.x + rec.width - outer;
    const float y0 = rec.y + outer, y1 = rec.y + rec.height - outer;
    use(white_texture, false);
    corner({x0, y0}, inner, outer, 180, segments, color);
    corner({x1, y0}, inner, outer, 270, segments, color);
    corner({x1, y1}, inner, outer, 0, segments, color);
    corner({x0, y1}, inner, outer, 90, segments, color);
    fill_rect({x0, rec.y, x1 - x0, band}, color);
    fill_rect({x0, rec.y + rec.height - band, x1 - x0, band}, color);
    fill_rect({rec.x, y0, band, y1 - y0}, color);
    fill_rect({rec.x + rec.width - band, y0, band, y1 - y0}, color);
}

void draw_text_ex(Font font, const char* text, Vec2 pos, float font_size, float spacing, Color tint) {
    if (font.texture.id == 0) {
        font = default_font;
    }
    if (!text || !font.glyphs || !font.recs) {
        return;
    }
    const float scale = font_size / static_cast<float>(font.baseSize);
    const float pad = static_cast<float>(font.glyphPadding);
    float ox = 0.0f, oy = 0.0f;
    GlyphLookup glyph_lookup(font, text);
    for (const char* p = text; *p;) {
        int n = 0;
        const int cp = get_codepoint_next(p, &n);
        p += n;
        if (cp == '\n') {
            oy += font_size + TEXT_LINE_SPACING;
            ox = 0.0f;
            continue;
        }
        const int gi = glyph_lookup.index(cp);
        const GlyphInfo& g = font.glyphs[gi];
        const Rect& r = font.recs[gi];
        if (cp != ' ' && cp != '\t') {
            draw_texture_pro(font.texture, {r.x - pad, r.y - pad, r.width + 2 * pad, r.height + 2 * pad},
                             {pos.x + ox + g.offsetX * scale - pad * scale,
                              pos.y + oy + g.offsetY * scale - pad * scale, (r.width + 2 * pad) * scale,
                              (r.height + 2 * pad) * scale},
                             {0, 0}, 0.0f, tint);
        }
        ox += g.advanceX == 0 ? r.width * scale + spacing : static_cast<float>(g.advanceX) * scale + spacing;
    }
}

void draw_text(const char* text, int x, int y, int font_size, Color color) {
    font_size = std::max(font_size, 10); // raylib's default-font minimum
    draw_text_ex(default_font, text, {static_cast<float>(x), static_cast<float>(y)}, static_cast<float>(font_size),
                 static_cast<float>(font_size / 10), color);
}

int measure_text(const char* text, int font_size) {
    font_size = std::max(font_size, 10);
    return static_cast<int>(
        measure_text_ex(default_font, text, static_cast<float>(font_size), static_cast<float>(font_size / 10)).x);
}

Font get_font_default() {
    return default_font;
}

// ---- 3D -------------------------------------------------------------------------------
// Models load from glTF with cgltf; GPU and fallback CPU skinning share raylib 6.2's semantics.
// (rmodels.c LoadGLTF / LoadModelAnimationsGLTF / UpdateModelAnimation): material 0 is a
// default one, node world transforms are baked into the vertices, animations are sampled at 60 fps.

namespace {

    constexpr int MAX_MATERIAL_MAPS = 12; // raylib's, so any map_type indexes safely
    constexpr float GLTF_FRAMERATE = 60.0f;

    // raylib's vertex buffer indices for update_mesh_buffer
    constexpr int MESH_BUFFER_POSITION = 0, MESH_BUFFER_TEXCOORD = 1, MESH_BUFFER_NORMAL = 2;

    Mat4 mat4_identity() {
        return matrix_scale(1, 1, 1);
    }

    std::array<float, 16> to_bgfx(const Mat4& m) { // column-major floats, like rlMatrixToFloat
        return {m.m0, m.m1, m.m2, m.m3, m.m4, m.m5, m.m6, m.m7, m.m8, m.m9, m.m10, m.m11, m.m12, m.m13, m.m14, m.m15};
    }

    Mat4 from_gltf(const float* f) { // cgltf column-major floats
        Mat4 m;
        m.m0 = f[0], m.m1 = f[1], m.m2 = f[2], m.m3 = f[3];
        m.m4 = f[4], m.m5 = f[5], m.m6 = f[6], m.m7 = f[7];
        m.m8 = f[8], m.m9 = f[9], m.m10 = f[10], m.m11 = f[11];
        m.m12 = f[12], m.m13 = f[13], m.m14 = f[14], m.m15 = f[15];
        return m;
    }

    Mat4 matrix_transpose(const Mat4& m) {
        Mat4 r;
        r.m0 = m.m0, r.m1 = m.m4, r.m2 = m.m8, r.m3 = m.m12;
        r.m4 = m.m1, r.m5 = m.m5, r.m6 = m.m9, r.m7 = m.m13;
        r.m8 = m.m2, r.m9 = m.m6, r.m10 = m.m10, r.m11 = m.m14;
        r.m12 = m.m3, r.m13 = m.m7, r.m14 = m.m11, r.m15 = m.m15;
        return r;
    }

    // raymath MatrixInvert
    Mat4 matrix_invert(const Mat4& mat) {
        const float a00 = mat.m0, a01 = mat.m1, a02 = mat.m2, a03 = mat.m3;
        const float a10 = mat.m4, a11 = mat.m5, a12 = mat.m6, a13 = mat.m7;
        const float a20 = mat.m8, a21 = mat.m9, a22 = mat.m10, a23 = mat.m11;
        const float a30 = mat.m12, a31 = mat.m13, a32 = mat.m14, a33 = mat.m15;
        const float b00 = a00 * a11 - a01 * a10, b01 = a00 * a12 - a02 * a10, b02 = a00 * a13 - a03 * a10;
        const float b03 = a01 * a12 - a02 * a11, b04 = a01 * a13 - a03 * a11, b05 = a02 * a13 - a03 * a12;
        const float b06 = a20 * a31 - a21 * a30, b07 = a20 * a32 - a22 * a30, b08 = a20 * a33 - a23 * a30;
        const float b09 = a21 * a32 - a22 * a31, b10 = a21 * a33 - a23 * a31, b11 = a22 * a33 - a23 * a32;
        const float inv = 1.0f / (b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06);
        Mat4 r;
        r.m0 = (a11 * b11 - a12 * b10 + a13 * b09) * inv;
        r.m1 = (-a01 * b11 + a02 * b10 - a03 * b09) * inv;
        r.m2 = (a31 * b05 - a32 * b04 + a33 * b03) * inv;
        r.m3 = (-a21 * b05 + a22 * b04 - a23 * b03) * inv;
        r.m4 = (-a10 * b11 + a12 * b08 - a13 * b07) * inv;
        r.m5 = (a00 * b11 - a02 * b08 + a03 * b07) * inv;
        r.m6 = (-a30 * b05 + a32 * b02 - a33 * b01) * inv;
        r.m7 = (a20 * b05 - a22 * b02 + a23 * b01) * inv;
        r.m8 = (a10 * b10 - a11 * b08 + a13 * b06) * inv;
        r.m9 = (-a00 * b10 + a01 * b08 - a03 * b06) * inv;
        r.m10 = (a30 * b04 - a31 * b02 + a33 * b00) * inv;
        r.m11 = (-a20 * b04 + a21 * b02 - a23 * b00) * inv;
        r.m12 = (-a10 * b09 + a11 * b07 - a12 * b06) * inv;
        r.m13 = (a00 * b09 - a01 * b07 + a02 * b06) * inv;
        r.m14 = (-a30 * b03 + a31 * b01 - a32 * b00) * inv;
        r.m15 = (a20 * b03 - a21 * b01 + a22 * b00) * inv;
        return r;
    }

    Vec3 v3_add(Vec3 a, Vec3 b) {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }
    Vec3 v3_sub(Vec3 a, Vec3 b) {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }
    Vec3 v3_mul(Vec3 a, Vec3 b) {
        return {a.x * b.x, a.y * b.y, a.z * b.z};
    }
    Vec3 v3_scale(Vec3 a, float s) {
        return {a.x * s, a.y * s, a.z * s};
    }
    float v3_dot(Vec3 a, Vec3 b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    Vec3 v3_cross(Vec3 a, Vec3 b) {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    Vec3 v3_lerp(Vec3 a, Vec3 b, float t) {
        return v3_add(a, v3_scale(v3_sub(b, a), t));
    }
    Vec3 v3_normalize(Vec3 v) {
        const float len = std::sqrt(v3_dot(v, v));
        return len == 0.0f ? v : v3_scale(v, 1.0f / len);
    }
    // the 3x3 part of m (normal transforms)
    Vec3 v3_transform_dir(Vec3 v, const Mat4& m) {
        return {m.m0 * v.x + m.m4 * v.y + m.m8 * v.z, m.m1 * v.x + m.m5 * v.y + m.m9 * v.z,
                m.m2 * v.x + m.m6 * v.y + m.m10 * v.z};
    }

    // raymath QuaternionMultiply
    Vec4 quat_mul(Vec4 a, Vec4 b) {
        return {a.x * b.w + a.w * b.x + a.y * b.z - a.z * b.y, a.y * b.w + a.w * b.y + a.z * b.x - a.x * b.z,
                a.z * b.w + a.w * b.z + a.x * b.y - a.y * b.x, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
    }

    // raymath Vector3RotateByQuaternion
    Vec3 quat_rotate(Vec3 v, Vec4 q) {
        return {v.x * (q.x * q.x + q.w * q.w - q.y * q.y - q.z * q.z) + v.y * (2 * q.x * q.y - 2 * q.w * q.z) +
                    v.z * (2 * q.x * q.z + 2 * q.w * q.y),
                v.x * (2 * q.w * q.z + 2 * q.x * q.y) + v.y * (q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z) +
                    v.z * (-2 * q.w * q.x + 2 * q.y * q.z),
                v.x * (-2 * q.w * q.y + 2 * q.x * q.z) + v.y * (2 * q.w * q.x + 2 * q.y * q.z) +
                    v.z * (q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z)};
    }

    // raymath QuaternionSlerp (with its nlerp for close rotations)
    Vec4 quat_slerp(Vec4 a, Vec4 b, float t) {
        float cos_half = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        if (cos_half < 0) {
            b = {-b.x, -b.y, -b.z, -b.w};
            cos_half = -cos_half;
        }
        if (std::fabs(cos_half) >= 1.0f) {
            return a;
        }
        if (cos_half > 0.95f) {
            Vec4 r{a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), a.z + t * (b.z - a.z), a.w + t * (b.w - a.w)};
            float len = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
            len = len == 0.0f ? 1.0f : 1.0f / len;
            return {r.x * len, r.y * len, r.z * len, r.w * len};
        }
        const float half = std::acos(cos_half), sin_half = std::sqrt(1.0f - cos_half * cos_half);
        if (std::fabs(sin_half) < 0.000001f) {
            return {a.x * 0.5f + b.x * 0.5f, a.y * 0.5f + b.y * 0.5f, a.z * 0.5f + b.z * 0.5f, a.w * 0.5f + b.w * 0.5f};
        }
        const float ra = std::sin((1 - t) * half) / sin_half, rb = std::sin(t * half) / sin_half;
        return {a.x * ra + b.x * rb, a.y * ra + b.y * rb, a.z * ra + b.z * rb, a.w * ra + b.w * rb};
    }

    // raymath QuaternionToMatrix
    Mat4 quat_to_matrix(Vec4 q) {
        Mat4 r = mat4_identity();
        const float a2 = q.x * q.x, b2 = q.y * q.y, c2 = q.z * q.z;
        const float ac = q.x * q.z, ab = q.x * q.y, bc = q.y * q.z;
        const float ad = q.w * q.x, bd = q.w * q.y, cd = q.w * q.z;
        r.m0 = 1 - 2 * (b2 + c2);
        r.m1 = 2 * (ab + cd);
        r.m2 = 2 * (ac - bd);
        r.m4 = 2 * (ab - cd);
        r.m5 = 1 - 2 * (a2 + c2);
        r.m6 = 2 * (bc + ad);
        r.m8 = 2 * (ac + bd);
        r.m9 = 2 * (bc - ad);
        r.m10 = 1 - 2 * (a2 + b2);
        return r;
    }

    // raymath QuaternionFromMatrix
    Vec4 quat_from_matrix(const Mat4& m) {
        const float w4 = m.m0 + m.m5 + m.m10, x4 = m.m0 - m.m5 - m.m10;
        const float y4 = m.m5 - m.m0 - m.m10, z4 = m.m10 - m.m0 - m.m5;
        int biggest = 0;
        float big4 = w4;
        if (x4 > big4) {
            big4 = x4;
            biggest = 1;
        }
        if (y4 > big4) {
            big4 = y4;
            biggest = 2;
        }
        if (z4 > big4) {
            big4 = z4;
            biggest = 3;
        }
        const float val = std::sqrt(big4 + 1.0f) * 0.5f, mult = 0.25f / val;
        switch (biggest) {
        case 0:
            return {(m.m6 - m.m9) * mult, (m.m8 - m.m2) * mult, (m.m1 - m.m4) * mult, val};
        case 1:
            return {val, (m.m1 + m.m4) * mult, (m.m8 + m.m2) * mult, (m.m6 - m.m9) * mult};
        case 2:
            return {(m.m1 + m.m4) * mult, val, (m.m6 + m.m9) * mult, (m.m8 - m.m2) * mult};
        default:
            return {(m.m8 + m.m2) * mult, (m.m6 + m.m9) * mult, val, (m.m1 - m.m4) * mult};
        }
    }

    // raymath MatrixDecompose
    Transform matrix_decompose(const Mat4& mat) {
        constexpr float eps = 1e-9f;
        Transform t;
        t.translation = {mat.m12, mat.m13, mat.m14};
        Vec3 c[3] = {{mat.m0, mat.m1, mat.m2}, {mat.m4, mat.m5, mat.m6}, {mat.m8, mat.m9, mat.m10}};
        float stab = eps;
        for (const Vec3& v : c) {
            stab = std::max({stab, std::fabs(v.x), std::fabs(v.y), std::fabs(v.z)});
        }
        for (Vec3& v : c) {
            v = v3_scale(v, 1.0f / stab);
        }
        Vec3 scl;
        scl.x = std::sqrt(v3_dot(c[0], c[0]));
        if (scl.x > eps) {
            c[0] = v3_scale(c[0], 1.0f / scl.x);
        }
        c[1] = v3_sub(c[1], v3_scale(c[0], v3_dot(c[0], c[1])));
        scl.y = std::sqrt(v3_dot(c[1], c[1]));
        if (scl.y > eps) {
            c[1] = v3_scale(c[1], 1.0f / scl.y);
        }
        c[2] = v3_sub(c[2], v3_scale(c[0], v3_dot(c[0], c[2])));
        c[2] = v3_sub(c[2], v3_scale(c[1], v3_dot(c[1], c[2])));
        scl.z = std::sqrt(v3_dot(c[2], c[2]));
        if (scl.z > eps) {
            c[2] = v3_scale(c[2], 1.0f / scl.z);
        }
        if (v3_dot(c[0], v3_cross(c[1], c[2])) < 0) {
            scl = v3_scale(scl, -1.0f);
            for (Vec3& v : c) {
                v = v3_scale(v, -1.0f);
            }
        }
        t.scale = v3_scale(scl, stab);
        Mat4 rot = mat4_identity();
        rot.m0 = c[0].x, rot.m4 = c[1].x, rot.m8 = c[2].x;
        rot.m1 = c[0].y, rot.m5 = c[1].y, rot.m9 = c[2].y;
        rot.m2 = c[0].z, rot.m6 = c[1].z, rot.m10 = c[2].z;
        t.rotation = quat_from_matrix(rot);
        return t;
    }

    // scale, then rotation, then translation (raylib's bone matrix order)
    Mat4 transform_matrix(const Transform& t) {
        return matrix_multiply(
            matrix_multiply(matrix_scale(t.scale.x, t.scale.y, t.scale.z), quat_to_matrix(t.rotation)),
            matrix_translate(t.translation.x, t.translation.y, t.translation.z));
    }

    // ---- GPU meshes ----

    struct MeshVertex {
        float x, y, z, nx, ny, nz, u, v;
        uint32_t rgba;
    };

    struct SkinVertex {
        std::array<uint8_t, 4> joints;
        std::array<float, 4> weights;
    };
    static_assert(sizeof(SkinVertex) == 20);

    struct SkeletonCache;
    struct MeshSlot {
        bgfx::DynamicVertexBufferHandle vb = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle ib = BGFX_INVALID_HANDLE;
        bgfx::VertexBufferHandle skin = BGFX_INVALID_HANDLE;
        std::vector<MeshVertex> cpu; // what vb holds, patched by update_mesh_buffer
        SkinningBounds skin_bounds;
        MeshBounds posed_bounds;
        SkeletonCache* palette = nullptr;
        bool cpu_deformed = false;
    };

    bgfx::VertexLayout mesh_layout, skin_layout;
    std::vector<MeshSlot> mesh_slots; // id = index + 1 (Mesh::vaoId)
    struct SkeletonCache {
        std::vector<Mat4> inverse_bind;
        std::vector<Mat4> normals;
        std::vector<std::array<float, 4>> position_rows;
        std::vector<std::array<float, 4>> normal_rows;
        std::span<const Mat4> matrices;
        IntegerPoseCache poses;
        std::shared_ptr<const CachedSkinPose> active_pose;
        uint64_t retained_frame = UINT64_MAX;
        const CachedSkinPose* retained_pose = nullptr;
    };
    std::unordered_map<Mat4*, SkeletonCache> skeleton_caches;
    ProgramSlot mesh_program; // materials without a shader of their own
    bgfx::UniformHandle u_col_diffuse = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_skin_position = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_skin_normal = BGFX_INVALID_HANDLE;

    // 3D mode and raylib's face culling state (on, back faces, front = counter-clockwise)
    bool backface_culling = true;
    bool cull_front = false;

    MeshSlot* mesh_slot(unsigned id) {
        return id && id <= mesh_slots.size() && bgfx::isValid(mesh_slots[id - 1].vb) ? &mesh_slots[id - 1] : nullptr;
    }

    void upload_mesh(Mesh& mesh) {
        if (mesh.vertexCount <= 0 || !mesh.vertices) {
            return;
        }
        MeshSlot slot;
        slot.cpu.resize(mesh.vertexCount);
        for (int i = 0; i < mesh.vertexCount; i++) {
            MeshVertex& v = slot.cpu[i];
            v.x = mesh.vertices[i * 3], v.y = mesh.vertices[i * 3 + 1], v.z = mesh.vertices[i * 3 + 2];
            if (mesh.normals) {
                v.nx = mesh.normals[i * 3], v.ny = mesh.normals[i * 3 + 1], v.nz = mesh.normals[i * 3 + 2];
            } else {
                v.nx = v.ny = v.nz = 0.0f;
            }
            if (mesh.texcoords) {
                v.u = mesh.texcoords[i * 2], v.v = mesh.texcoords[i * 2 + 1];
            } else {
                v.u = v.v = 0.0f;
            }
            v.rgba = mesh.colors ? static_cast<uint32_t>(mesh.colors[i * 4]) |
                                       static_cast<uint32_t>(mesh.colors[i * 4 + 1]) << 8 |
                                       static_cast<uint32_t>(mesh.colors[i * 4 + 2]) << 16 |
                                       static_cast<uint32_t>(mesh.colors[i * 4 + 3]) << 24
                                 : 0xffffffffu; // raylib's default for a missing colour attribute
        }
        slot.vb = bgfx::createDynamicVertexBuffer(
            bgfx::copy(slot.cpu.data(), static_cast<uint32_t>(slot.cpu.size() * sizeof(MeshVertex))), mesh_layout);
        if (caps->limits.maxVertexStreams >= 2 && caps->limits.maxVertexAttributes >= 6 &&
            bgfx::isValid(u_skin_position) && bgfx::isValid(u_skin_normal) && slot.skin_bounds.prepare(mesh)) {
            std::vector<SkinVertex> influences(mesh.vertexCount);
            for (int vertex = 0; vertex < mesh.vertexCount; ++vertex) {
                for (int influence = 0; influence < 4; ++influence) {
                    const auto joint = mesh.boneIndices[vertex * 4 + influence];
                    const bool valid = joint < mesh.boneCount;
                    influences[vertex].joints[influence] = valid ? joint : 0;
                    influences[vertex].weights[influence] = valid ? mesh.boneWeights[vertex * 4 + influence] : 0;
                }
            }
            slot.skin = bgfx::createVertexBuffer(
                bgfx::copy(influences.data(), static_cast<uint32_t>(influences.size() * sizeof(SkinVertex))),
                skin_layout);
        }
        if (mesh.indices) {
            slot.ib = bgfx::createIndexBuffer(
                bgfx::copy(mesh.indices, static_cast<uint32_t>(mesh.triangleCount * 3 * sizeof(unsigned short))));
        }
        mesh.vaoId = add_slot(mesh_slots, slot, [](const MeshSlot& s) { return bgfx::isValid(s.vb); });
    }

    void unload_mesh(Mesh& mesh) {
        if (MeshSlot* slot = mesh_slot(mesh.vaoId)) {
            bgfx::destroy(slot->vb);
            if (bgfx::isValid(slot->skin)) {
                bgfx::destroy(slot->skin);
            }
            if (bgfx::isValid(slot->ib)) {
                bgfx::destroy(slot->ib);
            }
            *slot = MeshSlot{};
        }
        for (void* p : {reinterpret_cast<void*>(mesh.vertices), reinterpret_cast<void*>(mesh.texcoords),
                        reinterpret_cast<void*>(mesh.texcoords2), reinterpret_cast<void*>(mesh.normals),
                        reinterpret_cast<void*>(mesh.tangents), reinterpret_cast<void*>(mesh.colors),
                        reinterpret_cast<void*>(mesh.indices), reinterpret_cast<void*>(mesh.boneIndices),
                        reinterpret_cast<void*>(mesh.boneWeights), reinterpret_cast<void*>(mesh.animVertices),
                        reinterpret_cast<void*>(mesh.animNormals), reinterpret_cast<void*>(mesh.vboId)}) {
            std::free(p);
        }
    }

    // raylib's GL state as bgfx state: face culling, then y-flipped targets reverse the winding
    uint64_t cull_state() {
        if (!backface_culling) {
            return 0;
        }
        return (cull_front != pass.flip) ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW;
    }

    void submit_mesh(const Mesh& mesh, const Material& material, const Mat4& transform, Color tint) {
        const MeshSlot* slot = mesh_slot(mesh.vaoId);
        if (!slot) {
            return;
        }
        flush();
        ProgramSlot& prog = is_shader_valid(material.shader) ? programs[material.shader.id] : mesh_program;
        const MaterialMap& diffuse = material.maps[material_map::DIFFUSE];
        const TextureSlot* tex = texture_slot(diffuse.texture.id);
        if (!tex) {
            tex = texture_slot(white_texture);
        }
        const Color c = diffuse.color;
        const float col[4] = {
            static_cast<float>(c.r * tint.r / 255) / 255.0f, static_cast<float>(c.g * tint.g / 255) / 255.0f,
            static_cast<float>(c.b * tint.b / 255) / 255.0f, static_cast<float>(c.a * tint.a / 255) / 255.0f};
        const auto model = to_bgfx(transform);
        bgfx::setTransform(model.data());
        bgfx::setVertexBuffer(0, slot->vb);
        const bool skinned = slot->palette && bgfx::isValid(slot->skin) && bgfx::isValid(prog.skinned);
        if (skinned) {
            bgfx::setVertexBuffer(1, slot->skin);
            auto& palette = *slot->palette;
            if (palette.active_pose) {
                if (palette.retained_frame != palette_frames.current_frame() ||
                    palette.retained_pose != palette.active_pose.get()) {
                    palette_frames.keep(palette.active_pose);
                    palette.retained_frame = palette_frames.current_frame();
                    palette.retained_pose = palette.active_pose.get();
                }
                const auto& pose = *palette.active_pose;
                const auto count = static_cast<uint16_t>(pose.position_rows.size());
                bgfx::setUniformRef(u_skin_position, pose.position_rows.data(), count);
                bgfx::setUniformRef(u_skin_normal, pose.normal_rows.data(), count);
            } else {
                const auto count = static_cast<uint16_t>(palette.position_rows.size());
                bgfx::setUniform(u_skin_position, palette.position_rows.data(), count);
                bgfx::setUniform(u_skin_normal, palette.normal_rows.data(), count);
            }
        }
        if (bgfx::isValid(slot->ib)) {
            bgfx::setIndexBuffer(slot->ib);
        }
        bgfx::setTexture(0, s_tex_color, tex->handle, sampler_flags(*tex));
        bgfx::setUniform(u_col_diffuse, col);
        for (size_t i = 0; i < prog.uniforms.size(); i++) {
            bgfx::setUniform(prog.uniforms[i], prog.values[i].data());
        }
        if (cur_scissor.on) {
            bgfx::setScissor(cur_scissor.x, cur_scissor.y, cur_scissor.w, cur_scissor.h);
        }
        const uint64_t depth = in_3d ? BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LEQUAL : 0;
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | depth | cur_blend | cull_state());
        bgfx::submit(pass.view, skinned ? prog.skinned : prog.program);
        submits_this_frame++;
        pass.submits++;
        screen_drawn |= pass.target == 0;
    }

    // ---- glTF ----

    // Reads `count` vec<n> floats of an attribute, normalizing integer data the way the accessor says.
    float* read_floats(const cgltf_accessor* acc, int n) {
        auto* out = static_cast<float*>(std::calloc(acc->count * n, sizeof(float)));
        cgltf_accessor_unpack_floats(acc, out, acc->count * n);
        return out;
    }

    Image load_gltf_image(const cgltf_image* img, const std::filesystem::path& dir) {
        if (!img) {
            return {};
        }
        if (img->buffer_view && img->buffer_view->buffer->data) {
            const auto* bytes =
                static_cast<const unsigned char*>(img->buffer_view->buffer->data) + img->buffer_view->offset;
            return load_image_from_memory({bytes, img->buffer_view->size});
        }
        if (img->uri && std::strncmp(img->uri, "data:", 5) != 0) {
            return load_image((dir / img->uri).string().c_str());
        }
        // ponytail: base64 data-URI images unsupported; the skins' .glb files embed buffer views
        spdlog::warn("MODEL: glTF data-URI images are not supported");
        return {};
    }

    BoneInfo* load_bones(const cgltf_skin& skin, unsigned& count) {
        count = static_cast<unsigned>(skin.joints_count);
        auto* bones = static_cast<BoneInfo*>(std::calloc(skin.joints_count, sizeof(BoneInfo)));
        for (size_t i = 0; i < skin.joints_count; i++) {
            const cgltf_node* node = skin.joints[i];
            if (node->name) {
                std::snprintf(bones[i].name, sizeof(bones[i].name), "%s", node->name);
            }
            // nearest ancestor that is also a joint (exporters put plain transform nodes between)
            bones[i].parent = -1;
            for (const cgltf_node* a = node->parent; a && bones[i].parent == -1; a = a->parent) {
                for (size_t j = 0; j < skin.joints_count; j++) {
                    if (skin.joints[j] == a) {
                        bones[i].parent = static_cast<int>(j);
                        break;
                    }
                }
            }
        }
        return bones;
    }

    struct GltfFile {
        cgltf_data* data = nullptr;
        explicit GltfFile(const char* path) {
            cgltf_options options{};
            if (cgltf_parse_file(&options, path, &data) != cgltf_result_success) {
                spdlog::warn("MODEL: [{}] Failed to load glTF data", path);
                data = nullptr;
            } else if (cgltf_load_buffers(&options, data, path) != cgltf_result_success) {
                spdlog::warn("MODEL: [{}] Failed to load glTF buffers", path);
                cgltf_free(data);
                data = nullptr;
            }
        }
        ~GltfFile() {
            if (data) {
                cgltf_free(data);
            }
        }
        GltfFile(const GltfFile&) = delete;
        GltfFile& operator=(const GltfFile&) = delete;
    };

    void load_primitive(Mesh& mesh, const cgltf_primitive& prim, const Mat4& world, const Mat4& world_normals) {
        for (size_t a = 0; a < prim.attributes_count; a++) {
            const cgltf_attribute& attr = prim.attributes[a];
            const cgltf_accessor* acc = attr.data;
            switch (attr.type) {
            case cgltf_attribute_type_position:
                if (acc->type == cgltf_type_vec3 && !mesh.vertices) {
                    mesh.vertexCount = static_cast<int>(acc->count);
                    mesh.vertices = read_floats(acc, 3);
                    for (int k = 0; k < mesh.vertexCount; k++) {
                        float* v = mesh.vertices + k * 3;
                        const Vec3 t = vector3_transform({v[0], v[1], v[2]}, world);
                        v[0] = t.x, v[1] = t.y, v[2] = t.z;
                    }
                }
                break;
            case cgltf_attribute_type_normal:
                if (acc->type == cgltf_type_vec3 && !mesh.normals) {
                    mesh.normals = read_floats(acc, 3);
                    for (size_t k = 0; k < acc->count; k++) {
                        float* n = mesh.normals + k * 3;
                        const Vec3 t = vector3_transform({n[0], n[1], n[2]}, world_normals);
                        n[0] = t.x, n[1] = t.y, n[2] = t.z;
                    }
                }
                break;
            case cgltf_attribute_type_texcoord:
                if (acc->type == cgltf_type_vec2 && attr.index <= 1) {
                    float*& dst = attr.index == 0 ? mesh.texcoords : mesh.texcoords2;
                    if (!dst) {
                        dst = read_floats(acc, 2);
                    }
                }
                break;
            case cgltf_attribute_type_color:
                if ((acc->type == cgltf_type_vec3 || acc->type == cgltf_type_vec4) && !mesh.colors) {
                    const int n = acc->type == cgltf_type_vec4 ? 4 : 3;
                    mesh.colors = static_cast<unsigned char*>(std::malloc(acc->count * 4));
                    for (size_t k = 0; k < acc->count; k++) {
                        unsigned char* c = mesh.colors + k * 4;
                        c[3] = 255;
                        if (acc->component_type == cgltf_component_type_r_32f) {
                            float f[4] = {};
                            cgltf_accessor_read_float(acc, k, f, n);
                            for (int i = 0; i < n; i++) {
                                c[i] = static_cast<unsigned char>(f[i] * 255.0f); // truncated, as raylib
                            }
                        } else {
                            cgltf_uint u[4] = {};
                            cgltf_accessor_read_uint(acc, k, u, n);
                            const bool wide = acc->component_type == cgltf_component_type_r_16u;
                            for (int i = 0; i < n; i++) {
                                c[i] = wide ? static_cast<unsigned char>(static_cast<float>(u[i]) / 65535.0f * 255.0f)
                                            : static_cast<unsigned char>(u[i]);
                            }
                        }
                    }
                }
                break;
            case cgltf_attribute_type_joints:
                if (acc->type == cgltf_type_vec4 && !mesh.boneIndices) {
                    mesh.boneIndices = static_cast<unsigned char*>(std::calloc(acc->count * 4, 1));
                    for (size_t k = 0; k < acc->count; k++) {
                        cgltf_uint u[4] = {};
                        cgltf_accessor_read_uint(acc, k, u, 4);
                        for (int i = 0; i < 4; i++) {
                            mesh.boneIndices[k * 4 + i] = static_cast<unsigned char>(u[i]); // raylib's u8 bone ids
                        }
                    }
                }
                break;
            case cgltf_attribute_type_weights:
                if (acc->type == cgltf_type_vec4 && !mesh.boneWeights) {
                    mesh.boneWeights = read_floats(acc, 4);
                }
                break;
            default:
                break;
            }
        }
        if (prim.indices && prim.indices->buffer_view) {
            const cgltf_accessor* acc = prim.indices;
            mesh.triangleCount = static_cast<int>(acc->count) / 3;
            mesh.indices = static_cast<unsigned short*>(std::malloc(acc->count * sizeof(unsigned short)));
            for (size_t k = 0; k < acc->count; k++) {
                mesh.indices[k] =
                    static_cast<unsigned short>(cgltf_accessor_read_index(acc, k)); // u32 truncated, as raylib
            }
        } else {
            mesh.triangleCount = mesh.vertexCount / 3;
        }
    }

    Material default_material() {
        Material m;
        m.maps = static_cast<MaterialMap*>(std::calloc(MAX_MATERIAL_MAPS, sizeof(MaterialMap)));
        m.maps[material_map::DIFFUSE].color = Color::WHITE;
        return m;
    }

    // raylib GetPoseAtTimeGLTF, step and linear only
    template<int N>
    void sample_channel(const cgltf_animation_channel* ch, float time, float* out) {
        if (!ch) {
            return;
        }
        const cgltf_accessor* input = ch->sampler->input;
        const cgltf_accessor* output = ch->sampler->output;
        if (output->component_type != cgltf_component_type_r_32f || input->count == 0) {
            return;
        }
        float t0 = 0.0f, t1 = 0.0f;
        int key = 0;
        bool found = false;
        for (int i = 0; i + 1 < static_cast<int>(input->count); i++) {
            cgltf_accessor_read_float(input, i, &t0, 1);
            cgltf_accessor_read_float(input, i + 1, &t1, 1);
            if (t0 <= time && time < t1) {
                key = i;
                found = true;
                break;
            }
        }
        if (!found && input->count >= 2) {
            key = static_cast<int>(input->count) - 2;
            float first = 0.0f;
            cgltf_accessor_read_float(input, 0, &first, 1);
            if (time < first) {
                key = 0;
            }
            cgltf_accessor_read_float(input, key, &t0, 1);
            cgltf_accessor_read_float(input, key + 1, &t1, 1);
        }
        // ponytail: cubic-spline channels sample as step; animations.glb is all LINEAR
        const bool step = ch->sampler->interpolation != cgltf_interpolation_type_linear ||
                          std::fabs(t1 - t0) <= 0.000001f * std::max(1.0f, std::max(std::fabs(t0), std::fabs(t1))) ||
                          input->count < 2;
        float a[4] = {}, b[4] = {};
        cgltf_accessor_read_float(output, key, a, N);
        if (step) {
            std::memcpy(out, a, N * sizeof(float));
            return;
        }
        cgltf_accessor_read_float(output, key + 1, b, N);
        const float t = std::clamp((time - t0) / std::max(t1 - t0, 0.000001f), 0.0f, 1.0f);
        if constexpr (N == 4) {
            const Vec4 q = quat_slerp({a[0], a[1], a[2], a[3]}, {b[0], b[1], b[2], b[3]}, t);
            out[0] = q.x, out[1] = q.y, out[2] = q.z, out[3] = q.w;
        } else {
            for (int i = 0; i < N; i++) {
                out[i] = a[i] + t * (b[i] - a[i]);
            }
        }
    }

    // raylib BuildPoseFromParentJoints: local joint transforms to model space
    void build_pose_from_parents(const BoneInfo* bones, int count, Transform* t) {
        for (int i = 0; i < count; i++) {
            const int p = bones[i].parent;
            if (p < 0 || p > i) {
                continue; // root, or not topologically sorted (raylib skips those too)
            }
            t[i].rotation = quat_mul(t[p].rotation, t[i].rotation);
            t[i].scale = v3_mul(t[i].scale, t[p].scale);
            t[i].translation =
                v3_add(quat_rotate(v3_mul(t[i].translation, t[p].scale), t[p].rotation), t[p].translation);
        }
    }

    class SkinWorker {
        std::mutex mutex;
        std::condition_variable ready, finished;
        std::function<void()> job;
        bool stopping = false;
        bool busy = false;
        std::thread thread;

    public:
        SkinWorker()
            : thread([this] {
                  platform_initialize_animation_worker();
                  std::unique_lock lock(mutex);
                  for (;;) {
                      ready.wait(lock, [this] { return stopping || busy; });
                      if (stopping) {
                          return;
                      }
                      lock.unlock();
                      job();
                      lock.lock();
                      job = {};
                      busy = false;
                      finished.notify_one();
                  }
              }) {}
        ~SkinWorker() {
            {
                std::scoped_lock lock(mutex);
                stopping = true;
            }
            ready.notify_one();
            thread.join();
        }
        void start(std::function<void()> work) {
            std::scoped_lock lock(mutex);
            job = std::move(work);
            busy = true;
            ready.notify_one();
        }
        void wait() {
            std::unique_lock lock(mutex);
            finished.wait(lock, [this] { return !busy; });
        }
    };

    // raylib UpdateModelAnimationVertexBuffers: CPU skinning into animVertices/animNormals
    void skin_meshes(Model& model, SkeletonCache& cache) {
        cache.matrices = {model.boneMatrices, model.skeleton.boneCount};
        auto& normal_mats = cache.normals;
        normal_mats.resize(model.skeleton.boneCount);
        cache.position_rows.resize(model.skeleton.boneCount * 3);
        cache.normal_rows.resize(model.skeleton.boneCount * 3);
        for (unsigned b = 0; b < model.skeleton.boneCount; b++) {
            normal_mats[b] = matrix_transpose(matrix_invert(model.boneMatrices[b]));
            const auto position = skin_matrix_rows(model.boneMatrices[b]);
            const auto normal = skin_matrix_rows(normal_mats[b]);
            std::ranges::copy(position, cache.position_rows.begin() + b * 3);
            std::ranges::copy(normal, cache.normal_rows.begin() + b * 3);
        }
        for (int m = 0; m < model.meshCount; m++) {
            Mesh& mesh = model.meshes[m];
            if (!mesh.boneWeights || !mesh.boneIndices) {
                continue;
            }
            MeshSlot* slot = mesh_slot(mesh.vaoId);
            const Material& material = model.materials[model.meshMaterial[m]];
            const ProgramSlot& program = is_shader_valid(material.shader) ? programs[material.shader.id] : mesh_program;
            if (slot && bgfx::isValid(slot->skin) && bgfx::isValid(program.skinned)) {
                if (slot->cpu_deformed) {
                    for (int vertex = 0; vertex < mesh.vertexCount; ++vertex) {
                        auto& v = slot->cpu[vertex];
                        v.x = mesh.vertices[vertex * 3];
                        v.y = mesh.vertices[vertex * 3 + 1];
                        v.z = mesh.vertices[vertex * 3 + 2];
                        if (mesh.normals) {
                            v.nx = mesh.normals[vertex * 3];
                            v.ny = mesh.normals[vertex * 3 + 1];
                            v.nz = mesh.normals[vertex * 3 + 2];
                        }
                    }
                    bgfx::update(
                        slot->vb, 0,
                        bgfx::copy(slot->cpu.data(), static_cast<uint32_t>(slot->cpu.size() * sizeof(MeshVertex))));
                    slot->cpu_deformed = false;
                }
                slot->palette = &cache;
                slot->posed_bounds = slot->skin_bounds.posed(cache.matrices);
                continue;
            }
            if (slot) {
                slot->palette = nullptr;
                slot->cpu_deformed = true;
            }
            const size_t floats = static_cast<size_t>(mesh.vertexCount) * 3;
            if (!mesh.animVertices) {
                mesh.animVertices = static_cast<float*>(std::malloc(floats * sizeof(float)));
            }
            if (mesh.normals && !mesh.animNormals) {
                mesh.animNormals = static_cast<float*>(std::malloc(floats * sizeof(float)));
            }
            bool changed = false;
            auto skin_range = [&](int begin, int end) {
                bool range_changed = false;
                for (int v = begin; v < end; v++) {
                    Vec3 pos{}, nrm{};
                    const Vec3 src{mesh.vertices[v * 3], mesh.vertices[v * 3 + 1], mesh.vertices[v * 3 + 2]};
                    const Vec3 src_n = mesh.normals
                                           ? Vec3{mesh.normals[v * 3], mesh.normals[v * 3 + 1], mesh.normals[v * 3 + 2]}
                                           : Vec3{};
                    for (int j = 0; j < 4; j++) {
                        const float w = mesh.boneWeights[v * 4 + j];
                        const unsigned b = mesh.boneIndices[v * 4 + j];
                        if (w == 0.0f || b >= model.skeleton.boneCount) {
                            continue;
                        }
                        pos = v3_add(pos, v3_scale(vector3_transform(src, model.boneMatrices[b]), w));
                        if (mesh.animNormals) {
                            nrm = v3_add(nrm, v3_scale(v3_transform_dir(src_n, normal_mats[b]), w));
                        }
                        range_changed = true;
                    }
                    mesh.animVertices[v * 3] = pos.x, mesh.animVertices[v * 3 + 1] = pos.y,
                                          mesh.animVertices[v * 3 + 2] = pos.z;
                    if (mesh.animNormals) {
                        mesh.animNormals[v * 3] = nrm.x, mesh.animNormals[v * 3 + 1] = nrm.y,
                                             mesh.animNormals[v * 3 + 2] = nrm.z;
                    }
                    if (slot && v < static_cast<int>(slot->cpu.size())) {
                        auto& vertex = slot->cpu[v];
                        vertex.x = pos.x;
                        vertex.y = pos.y;
                        vertex.z = pos.z;
                        if (mesh.animNormals) {
                            vertex.nx = nrm.x;
                            vertex.ny = nrm.y;
                            vertex.nz = nrm.z;
                        }
                    }
                }
                return range_changed;
            };
            if (platform_threaded_renderer() && mesh.vertexCount >= 2048) {
                static SkinWorker worker;
                bool worker_changed = false;
                const int middle = mesh.vertexCount / 2;
                worker.start([&] { worker_changed = skin_range(0, middle); });
                changed = skin_range(middle, mesh.vertexCount);
                worker.wait();
                changed = changed || worker_changed;
            } else {
                changed = skin_range(0, mesh.vertexCount);
            }
            if (changed && slot) {
                bgfx::update(
                    slot->vb, 0,
                    bgfx::copy(slot->cpu.data(), static_cast<uint32_t>(slot->cpu.size() * sizeof(MeshVertex))));
            }
        }
    }

} // namespace

Model load_model(const char* path) {
    Model model;
    model.transform = mat4_identity();
    GltfFile file(path);
    const cgltf_data* data = file.data;
    if (!data) {
        return model;
    }

    for (size_t i = 0; i < data->nodes_count; i++) {
        if (const cgltf_mesh* mesh = data->nodes[i].mesh) {
            for (size_t p = 0; p < mesh->primitives_count; p++) {
                if (mesh->primitives[p].has_draco_mesh_compression) {
                    spdlog::warn("MODEL: [{}] Draco compression is not supported", path);
                    return model;
                }
                model.meshCount += mesh->primitives[p].type == cgltf_primitive_type_triangles;
            }
        }
    }
    model.meshes = static_cast<Mesh*>(std::calloc(std::max(model.meshCount, 1), sizeof(Mesh)));
    model.meshMaterial = static_cast<int*>(std::calloc(std::max(model.meshCount, 1), sizeof(int)));

    // material 0 is the default one; glTF material i is i + 1
    const std::filesystem::path dir = std::filesystem::path(path).parent_path();
    model.materialCount = static_cast<int>(data->materials_count) + 1;
    model.materials = static_cast<Material*>(std::calloc(model.materialCount, sizeof(Material)));
    model.materials[0] = default_material();
    for (size_t i = 0; i < data->materials_count; i++) {
        Material& mat = model.materials[i + 1];
        mat = default_material();
        const cgltf_material& src = data->materials[i];
        if (!src.has_pbr_metallic_roughness) {
            continue;
        }
        const cgltf_pbr_metallic_roughness& pbr = src.pbr_metallic_roughness;
        if (pbr.base_color_texture.texture) {
            Image img = load_gltf_image(pbr.base_color_texture.texture->image, dir);
            if (img.data) {
                mat.maps[material_map::DIFFUSE].texture = load_texture_from_image(img);
                unload_image(img);
            }
        }
        Color& c = mat.maps[material_map::DIFFUSE].color;
        c = Color(
            static_cast<uint8_t>(pbr.base_color_factor[0] * 255), static_cast<uint8_t>(pbr.base_color_factor[1] * 255),
            static_cast<uint8_t>(pbr.base_color_factor[2] * 255), static_cast<uint8_t>(pbr.base_color_factor[3] * 255));
    }

    const cgltf_skin* skin = data->skins_count > 0 ? &data->skins[0] : nullptr;
    int mesh_index = 0;
    for (size_t i = 0; i < data->nodes_count; i++) {
        const cgltf_node* node = &data->nodes[i];
        if (!node->mesh) {
            continue;
        }
        float wt[16];
        cgltf_node_transform_world(node, wt);
        const Mat4 world = from_gltf(wt);
        const Mat4 world_normals = matrix_transpose(matrix_invert(world));
        for (size_t p = 0; p < node->mesh->primitives_count; p++) {
            const cgltf_primitive& prim = node->mesh->primitives[p];
            if (prim.type != cgltf_primitive_type_triangles) {
                continue;
            }
            Mesh& mesh = model.meshes[mesh_index];
            load_primitive(mesh, prim, world, world_normals);
            if (prim.material) {
                model.meshMaterial[mesh_index] = static_cast<int>(prim.material - data->materials) + 1;
            }
            // a rigid mesh parented to a joint follows that joint
            if (skin && !mesh.boneIndices && node->parent && !node->parent->mesh) {
                for (size_t j = 0; j < skin->joints_count; j++) {
                    if (skin->joints[j] == node->parent) {
                        mesh.boneIndices = static_cast<unsigned char*>(std::calloc(mesh.vertexCount * 4, 1));
                        mesh.boneWeights = static_cast<float*>(std::calloc(mesh.vertexCount * 4, sizeof(float)));
                        for (int v = 0; v < mesh.vertexCount; v++) {
                            mesh.boneIndices[v * 4] = static_cast<unsigned char>(j);
                            mesh.boneWeights[v * 4] = 1.0f;
                        }
                        break;
                    }
                }
            }
            mesh_index++;
        }
    }

    if (skin) {
        model.skeleton.bones = load_bones(*skin, model.skeleton.boneCount);
        model.skeleton.bindPose = static_cast<Transform*>(std::calloc(model.skeleton.boneCount, sizeof(Transform)));
        for (unsigned i = 0; i < model.skeleton.boneCount; i++) {
            float m[16] = {};
            Mat4 bind;
            if (skin->inverse_bind_matrices && skin->inverse_bind_matrices->count >= skin->joints_count &&
                cgltf_accessor_read_float(skin->inverse_bind_matrices, i, m, 16)) {
                bind = matrix_invert(from_gltf(m));
            } else {
                cgltf_node_transform_world(skin->joints[i], m);
                bind = from_gltf(m);
            }
            model.skeleton.bindPose[i] = matrix_decompose(bind);
        }
        if (model.skeleton.boneCount > 0) {
            model.currentPose = static_cast<Transform*>(std::calloc(model.skeleton.boneCount, sizeof(Transform)));
            model.boneMatrices = static_cast<Mat4*>(std::calloc(model.skeleton.boneCount, sizeof(Mat4)));
            for (unsigned b = 0; b < model.skeleton.boneCount; b++) {
                model.boneMatrices[b] = mat4_identity();
            }
        }
    }
    for (int m = 0; m < model.meshCount; m++) {
        model.meshes[m].boneCount = static_cast<int>(model.skeleton.boneCount);
        upload_mesh(model.meshes[m]);
    }
    return model;
}

void unload_model(Model model) {
    skeleton_caches.erase(model.boneMatrices);
    for (int i = 0; i < model.meshCount; i++) {
        unload_mesh(model.meshes[i]);
    }
    // Unlike raylib, the model owns the textures its materials hold when it is unloaded (the
    // game swaps its own in and clears shared ones, e.g. Chara3D's face sheet, beforehand).
    for (int i = 0; i < model.materialCount; i++) {
        if (model.materials[i].maps) {
            unload_texture(model.materials[i].maps[material_map::DIFFUSE].texture);
            std::free(model.materials[i].maps);
        }
    }
    for (void* p : {reinterpret_cast<void*>(model.meshes), reinterpret_cast<void*>(model.materials),
                    reinterpret_cast<void*>(model.meshMaterial), reinterpret_cast<void*>(model.skeleton.bones),
                    reinterpret_cast<void*>(model.skeleton.bindPose), reinterpret_cast<void*>(model.currentPose),
                    reinterpret_cast<void*>(model.boneMatrices)}) {
        std::free(p);
    }
}

ModelAnimation* load_model_animations(const char* path, int* count) {
    *count = 0;
    GltfFile file(path);
    const cgltf_data* data = file.data;
    if (!data || data->skins_count == 0 || data->animations_count == 0) {
        return nullptr;
    }
    const cgltf_skin& skin = data->skins[0];
    const size_t joints = skin.joints_count;

    // transforms of the plain (non-joint) nodes above each joint, which raylib bakes into its pose
    std::vector<Mat4> ext_offset(joints, mat4_identity());
    for (size_t k = 0; k < joints; k++) {
        for (const cgltf_node* n = skin.joints[k]->parent; n; n = n->parent) {
            if (std::find(skin.joints, skin.joints + joints, n) != skin.joints + joints) {
                break;
            }
            const Transform t{{n->translation[0], n->translation[1], n->translation[2]},
                              {n->rotation[0], n->rotation[1], n->rotation[2], n->rotation[3]},
                              {n->scale[0], n->scale[1], n->scale[2]}};
            ext_offset[k] = matrix_multiply(ext_offset[k], transform_matrix(t));
        }
    }

    auto* anims = static_cast<ModelAnimation*>(std::calloc(data->animations_count, sizeof(ModelAnimation)));
    struct Channels {
        const cgltf_animation_channel *translate = nullptr, *rotate = nullptr, *scale = nullptr;
    };
    for (size_t a = 0; a < data->animations_count; a++) {
        const cgltf_animation& src = data->animations[a];
        ModelAnimation& anim = anims[a];
        BoneInfo* bones = load_bones(skin, anim.boneCount);
        std::vector<Channels> channels(joints);
        float duration = 0.0f;
        for (size_t c = 0; c < src.channels_count; c++) {
            const cgltf_animation_channel& ch = src.channels[c];
            const auto* it = std::find(skin.joints, skin.joints + joints, ch.target_node);
            if (it == skin.joints + joints) {
                continue;
            }
            Channels& bc = channels[it - skin.joints];
            if (ch.target_path == cgltf_animation_path_type_translation) {
                bc.translate = &ch;
            } else if (ch.target_path == cgltf_animation_path_type_rotation) {
                bc.rotate = &ch;
            } else if (ch.target_path == cgltf_animation_path_type_scale) {
                bc.scale = &ch;
            }
            float end = 0.0f;
            if (ch.sampler->input->count > 0 &&
                cgltf_accessor_read_float(ch.sampler->input, ch.sampler->input->count - 1, &end, 1)) {
                duration = std::max(duration, end);
            }
        }
        if (src.name) {
            std::snprintf(anim.name, sizeof(anim.name), "%s", src.name);
        }
        anim.keyframeCount = static_cast<int>(duration * GLTF_FRAMERATE) + 1;
        anim.keyframePoses = static_cast<Transform**>(std::calloc(anim.keyframeCount, sizeof(Transform*)));
        for (int f = 0; f < anim.keyframeCount; f++) {
            Transform* pose = anim.keyframePoses[f] = static_cast<Transform*>(std::calloc(joints, sizeof(Transform)));
            const float time = static_cast<float>(f) / GLTF_FRAMERATE;
            for (size_t k = 0; k < joints; k++) {
                const cgltf_node* j = skin.joints[k];
                float tr[3] = {j->translation[0], j->translation[1], j->translation[2]};
                float rot[4] = {j->rotation[0], j->rotation[1], j->rotation[2], j->rotation[3]};
                float sc[3] = {j->scale[0], j->scale[1], j->scale[2]};
                sample_channel<3>(channels[k].translate, time, tr);
                sample_channel<4>(channels[k].rotate, time, rot);
                sample_channel<3>(channels[k].scale, time, sc);
                const Transform local{{tr[0], tr[1], tr[2]}, {rot[0], rot[1], rot[2], rot[3]}, {sc[0], sc[1], sc[2]}};
                pose[k] = matrix_decompose(matrix_multiply(transform_matrix(local), ext_offset[k]));
            }
            build_pose_from_parents(bones, static_cast<int>(anim.boneCount), pose);
        }
        std::free(bones);
    }
    *count = static_cast<int>(data->animations_count);
    return anims;
}

void unload_model_animations(ModelAnimation* anims, int count) {
    if (!anims) {
        return;
    }
    for (auto& [key, cache] : skeleton_caches) {
        cache.poses.clear();
    }
    for (int a = 0; a < count; a++) {
        for (int f = 0; f < anims[a].keyframeCount; f++) {
            std::free(anims[a].keyframePoses[f]);
        }
        std::free(anims[a].keyframePoses);
    }
    std::free(anims);
}

void update_model_animation(Model model, ModelAnimation anim, float frame) {
    if (anim.keyframeCount <= 0 || !anim.keyframePoses || !model.skeleton.bones || !model.boneMatrices) {
        return;
    }
    // raylib UpdateModelAnimationBones: blend two keyframes, bone matrix = inverse(bind) * pose
    int cur = static_cast<int>(frame), next = cur + 1;
    const float blend = std::clamp(frame - static_cast<float>(cur), 0.0f, 1.0f);
    cur %= anim.keyframeCount;
    next %= anim.keyframeCount;
    const Transform* a = anim.keyframePoses[cur];
    const Transform* b = anim.keyframePoses[next];
    if (!a || !b) {
        return;
    }
    auto& cache = skeleton_caches[model.boneMatrices];
    cache.active_pose.reset();
    bool cache_pose = blend == 0.0f && anim.boneCount >= model.skeleton.boneCount;
    if (cache_pose) {
        for (int m = 0; m < model.meshCount; ++m) {
            const auto& mesh = model.meshes[m];
            if (!mesh.boneIndices || !mesh.boneWeights) {
                continue;
            }
            const auto* slot = mesh_slot(mesh.vaoId);
            const auto& material = model.materials[model.meshMaterial[m]];
            const auto& program = is_shader_valid(material.shader) ? programs[material.shader.id] : mesh_program;
            if (!slot || slot->cpu_deformed || !bgfx::isValid(slot->skin) || !bgfx::isValid(program.skinned)) {
                cache.poses.clear();
                cache_pose = false;
                break;
            }
        }
    }
    cache_pose = cache_pose &&
                 cache.poses.select(anim.keyframePoses, anim.keyframeCount, model.skeleton.boneCount, model.meshCount);
    if (cache_pose) {
        if (auto cached = cache.poses.get(cur)) {
            std::ranges::copy(cached->pose, model.currentPose);
            std::ranges::copy(cached->matrices, model.boneMatrices);
            cache.active_pose = cached;
            cache.matrices = {model.boneMatrices, model.skeleton.boneCount};
            for (int m = 0; m < model.meshCount; ++m) {
                if (auto* slot = mesh_slot(model.meshes[m].vaoId); slot && cached->bounds[m].valid) {
                    slot->palette = &cache;
                    slot->posed_bounds = cached->bounds[m];
                }
            }
            return;
        }
    }
    if (cache.inverse_bind.size() != model.skeleton.boneCount) {
        cache.inverse_bind.resize(model.skeleton.boneCount);
        for (unsigned i = 0; i < model.skeleton.boneCount; ++i) {
            cache.inverse_bind[i] = matrix_invert(transform_matrix(model.skeleton.bindPose[i]));
        }
    }
    for (unsigned i = 0; i < model.skeleton.boneCount && i < anim.boneCount; i++) {
        Transform& pose = model.currentPose[i];
        pose.translation = v3_lerp(a[i].translation, b[i].translation, blend);
        pose.rotation = quat_slerp(a[i].rotation, b[i].rotation, blend);
        pose.scale = v3_lerp(a[i].scale, b[i].scale, blend);
        model.boneMatrices[i] = matrix_multiply(cache.inverse_bind[i], transform_matrix(pose));
    }
    skin_meshes(model, cache);
    if (cache_pose) {
        auto cached = std::make_shared<CachedSkinPose>();
        cached->pose.assign(model.currentPose, model.currentPose + model.skeleton.boneCount);
        cached->matrices.assign(model.boneMatrices, model.boneMatrices + model.skeleton.boneCount);
        cached->position_rows = cache.position_rows;
        cached->normal_rows = cache.normal_rows;
        cached->bounds.resize(model.meshCount);
        for (int m = 0; m < model.meshCount; ++m) {
            if (const auto* slot = mesh_slot(model.meshes[m].vaoId); slot && slot->palette) {
                cached->bounds[m] = slot->posed_bounds;
            }
        }
        cache.active_pose = cached;
        cache.poses.put(cur, std::move(cached));
    }
}

bool get_mesh_bounds(const Mesh& mesh, Vec3& lower, Vec3& upper) {
    if (const auto* slot = mesh_slot(mesh.vaoId); slot && slot->palette && slot->posed_bounds.valid) {
        lower = slot->posed_bounds.lower;
        upper = slot->posed_bounds.upper;
        return true;
    }
    return cpu_mesh_bounds(mesh, lower, upper);
}

void update_mesh_buffer(Mesh mesh, int index, const void* data, int data_size, int offset) {
    MeshSlot* slot = mesh_slot(mesh.vaoId);
    if (!slot || !data) {
        return;
    }
    const int comps = index == MESH_BUFFER_TEXCOORD ? 2 : 3;
    auto field = [&](MeshVertex& v) {
        return index == MESH_BUFFER_POSITION ? &v.x : index == MESH_BUFFER_NORMAL ? &v.nx : &v.u;
    };
    if (index != MESH_BUFFER_POSITION && index != MESH_BUFFER_NORMAL && index != MESH_BUFFER_TEXCOORD) {
        spdlog::warn("bgfx: update_mesh_buffer index {} not supported", index); // ponytail: colours never change
        return;
    }
    const int stride = comps * static_cast<int>(sizeof(float));
    const int first = offset / stride;
    const int n = std::min(data_size / stride, static_cast<int>(slot->cpu.size()) - first);
    const auto* src = static_cast<const float*>(data);
    for (int i = 0; i < n; i++) {
        std::memcpy(field(slot->cpu[first + i]), src + i * comps, stride);
    }
    if (n > 0) {
        if (slot->palette) {
            slot->palette->poses.clear();
        }
        if (index == MESH_BUFFER_POSITION && bgfx::isValid(slot->skin)) {
            slot->skin_bounds.prepare(mesh);
            if (slot->palette) {
                slot->posed_bounds = slot->skin_bounds.posed(slot->palette->matrices);
            }
        }
        bgfx::update(slot->vb, static_cast<uint32_t>(first),
                     bgfx::copy(slot->cpu.data() + first, static_cast<uint32_t>(n * sizeof(MeshVertex))));
    }
}

void set_material_texture(Material* material, int map_type, Texture texture) {
    material->maps[map_type].texture = texture;
}

void draw_mesh(Mesh mesh, Material material, Mat4 transform) {
    submit_mesh(mesh, material, transform, Color::WHITE);
}

void draw_model(Model model, Vec3 pos, float scale, Color tint) {
    // raylib DrawModelEx without rotation: model.transform, then scale, then translation
    const Mat4 transform = matrix_multiply(
        model.transform, matrix_multiply(matrix_scale(scale, scale, scale), matrix_translate(pos.x, pos.y, pos.z)));
    for (int i = 0; i < model.meshCount; i++) {
        submit_mesh(model.meshes[i], model.materials[model.meshMaterial[i]], transform, tint);
    }
}

void begin_mode_3d(Camera3D camera) {
    open_pass(pass.target); // a view of its own for the camera's transforms
    in_3d = true;
    // raylib BeginMode3D: right-handed, near/far 0.05/4000, aspect of the current target
    constexpr float near = 0.05f, far = 4000.0f;
    const float aspect = static_cast<float>(pass.width) / static_cast<float>(std::max(pass.height, 1));
    float view[16], proj[16];
    bx::mtxLookAt(view, {camera.position.x, camera.position.y, camera.position.z},
                  {camera.target.x, camera.target.y, camera.target.z}, {camera.up.x, camera.up.y, camera.up.z},
                  bx::Handedness::Right);
    if (camera.projection == camera_projection::ORTHOGRAPHIC) {
        const float top = camera.fovy / 2.0f, right = top * aspect;
        bx::mtxOrtho(proj, -right, right, -top, top, near, far, 0.0f, caps->homogeneousDepth, bx::Handedness::Right);
    } else {
        bx::mtxProj(proj, camera.fovy, aspect, near, far, caps->homogeneousDepth, bx::Handedness::Right);
    }
    if (pass.flip) { // render targets are stored bottom-up, as in GL
        for (int i : {1, 5, 9, 13}) {
            proj[i] = -proj[i];
        }
    }
    bgfx::setViewTransform(pass.view, view, proj);
}

void end_mode_3d() {
    in_3d = false;
    open_pass(pass.target);
}

void set_backface_culling(bool enabled) {
    backface_culling = enabled;
}

void set_cull_face(int face) {
    cull_front = face == cull_face::FRONT;
}

namespace {

    void init_3d() {
        mesh_layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();
        skin_layout.begin()
            .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Uint8, false, true)
            .add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Float)
            .end();
        mesh_program = make_mesh_program(vs_mesh_spv, sizeof(vs_mesh_spv), vs_mesh_skin_spv, sizeof(vs_mesh_skin_spv),
                                         fs_mesh_spv, sizeof(fs_mesh_spv));
        u_col_diffuse = bgfx::createUniform("colDiffuse", bgfx::UniformType::Vec4);
        u_skin_position = bgfx::createUniform("u_skinPosition", bgfx::UniformType::Vec4, GPU_SKIN_BONES * 3);
        u_skin_normal = bgfx::createUniform("u_skinNormal", bgfx::UniformType::Vec4, GPU_SKIN_BONES * 3);
    }

    void shutdown_3d() {
        for (auto& m : mesh_slots) {
            if (bgfx::isValid(m.skin)) {
                bgfx::destroy(m.skin);
            }
            if (bgfx::isValid(m.vb)) {
                bgfx::destroy(m.vb);
            }
            if (bgfx::isValid(m.ib)) {
                bgfx::destroy(m.ib);
            }
        }
        mesh_slots.clear();
        bgfx::destroy(mesh_program.program);
        if (bgfx::isValid(mesh_program.skinned)) {
            bgfx::destroy(mesh_program.skinned);
        }
        bgfx::destroy(u_col_diffuse);
        bgfx::destroy(u_skin_position);
        bgfx::destroy(u_skin_normal);
        skeleton_caches.clear();
    }

} // namespace
