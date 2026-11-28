#include "chara_3d.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <numbers>
#include <numeric>
#include <optional>
#include <rapidjson/document.h>

#include "../../libs/file/filesystem.h"
#include "../../libs/file/scores.h"
#include "../../libs/gfx.h"
#include "../../libs/gfx/camera_utils.h"
#include "../../platform/platform.h"

static void draw_model_face_last(const Model& model, int face_material_index, const std::vector<int>& blend_materials,
                                 const std::vector<int>& twosided_materials, const Mat4& transform) {
    auto is_blend = [&](int mat) { return std::ranges::contains(blend_materials, mat); };
    // _CULLNONE materials are drawn two-sided, as the name asks.
    auto draw = [&](int i) {
        const int mat = model.meshMaterial[i];
        const bool two_sided = std::ranges::contains(twosided_materials, mat);
        if (two_sided) {
            set_backface_culling(false);
        }
        draw_mesh(model.meshes[i], model.materials[mat], transform);
        if (two_sided) {
            set_backface_culling(true);
        }
    };

    // 1. opaque and alpha-tested meshes
    for (int i = 0; i < model.meshCount; i++) {
        const int mat = model.meshMaterial[i];
        if (mat == face_material_index || is_blend(mat)) {
            continue;
        }
        draw(i);
    }
    // 2. the face plane
    if (face_material_index != -1) {
        for (int i = 0; i < model.meshCount; i++) {
            if (model.meshMaterial[i] == face_material_index) {
                draw_mesh(model.meshes[i], model.materials[model.meshMaterial[i]], transform);
            }
        }
    }
    // 3. alpha-blended sheets (_A_AB / glTF BLEND: front hair, plates, glints) last. Drawn
    //    before the face, their fully transparent texels still wrote depth and occluded the face
    //    plane and the drum head behind them, so the face came out as the black hull. They keep
    //    writing depth among themselves, as before, so a plate in front of a hair sheet stays on top.
    for (int i = 0; i < model.meshCount; i++) {
        const int mat = model.meshMaterial[i];
        if (mat != face_material_index && is_blend(mat)) {
            draw(i);
        }
    }
}

static Mat4 rotation_xyz(float ax, float ay, float az) {
    float cx = cosf(-ax), sx = sinf(-ax);
    float cy = cosf(-ay), sy = sinf(-ay);
    float cz = cosf(-az), sz = sinf(-az);
    Mat4 r = {};
    r.m0 = cz * cy;
    r.m1 = (cz * sy * sx) - (sz * cx);
    r.m2 = (cz * sy * cx) + (sz * sx);
    r.m4 = sz * cy;
    r.m5 = (sz * sy * sx) + (cz * cx);
    r.m6 = (sz * sy * cx) - (cz * sx);
    r.m8 = -sy;
    r.m9 = cy * sx;
    r.m10 = cy * cx;
    r.m15 = 1.0f;
    return r;
}

struct AnimSource {
    fs::file_time_type mtime;
    std::uintmax_t size = 0;
    std::vector<std::string> bone_names; // the glb's skeleton; empty if it has none
    ModelAnimation* anims = nullptr;
    int count = 0;
    std::vector<std::vector<int>> faces; // [animation][60 fps frame] expression index
};

static std::vector<std::vector<int>> read_glb_faces(const fs::path& path, const ModelAnimation* anims, int count) {
    std::vector<std::vector<int>> faces(count);
    std::ifstream f(path, std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto u32 = [&](size_t at) {
        uint32_t v = 0;
        std::memcpy(&v, data.data() + at, 4);
        return v;
    };
    if (data.size() < 20 || u32(0) != 0x46546C67u || u32(16) != 0x4E4F534Au) {
        return faces;
    }
    const size_t json_len = u32(12);
    const size_t bin_at = 20 + json_len + 8;
    if (bin_at > data.size()) {
        return faces;
    }
    rapidjson::Document doc;
    doc.Parse(data.data() + 20, json_len);
    if (doc.HasParseError() || !doc.HasMember("nodes") || !doc.HasMember("animations")) {
        return faces;
    }
    int extra = -1;
    for (rapidjson::SizeType i = 0; i < doc["nodes"].Size(); i++) {
        const auto& node = doc["nodes"][i];
        if (node.HasMember("name") && std::strcmp(node["name"].GetString(), "EXTRA") == 0) {
            extra = static_cast<int>(i);
        }
    }
    if (extra < 0) {
        return faces;
    }
    // first float of element k of a float accessor, or nullopt past the buffer
    auto read = [&](int accessor, int k) -> std::optional<float> {
        const auto& acc = doc["accessors"][accessor];
        const auto& view = doc["bufferViews"][acc["bufferView"].GetInt()];
        const int comps = std::strcmp(acc["type"].GetString(), "VEC3") == 0 ? 3 : 1;
        const size_t stride = view.HasMember("byteStride") ? view["byteStride"].GetUint() : comps * 4;
        const size_t at = bin_at + (view.HasMember("byteOffset") ? view["byteOffset"].GetUint() : 0) +
                          (acc.HasMember("byteOffset") ? acc["byteOffset"].GetUint() : 0) + k * stride;
        if (at + 4 > data.size()) {
            return std::nullopt;
        }
        float v = 0;
        std::memcpy(&v, data.data() + at, 4);
        return v;
    };
    const auto& animations = doc["animations"];
    for (int a = 0; a < std::min(count, static_cast<int>(animations.Size())); a++) {
        const auto& anim = animations[a];
        for (const auto& ch : anim["channels"].GetArray()) {
            const auto& target = ch["target"];
            if (!target.HasMember("node") || target["node"].GetInt() != extra ||
                std::strcmp(target["path"].GetString(), "translation") != 0) {
                continue;
            }
            const auto& sampler = anim["samplers"][ch["sampler"].GetInt()];
            const int in = sampler["input"].GetInt();
            const int out = sampler["output"].GetInt();
            const int keys = doc["accessors"][in]["count"].GetInt();
            auto& track = faces[a];
            int k = 0;
            for (int frame = 0; frame < anims[a].keyframeCount; frame++) {
                const float t = static_cast<float>(frame) / 60.0f;
                while (k + 1 < keys && read(in, k + 1).value_or(t) <= t) {
                    k++;
                }
                float x = read(out, k).value_or(0.0f);
                if (k + 1 < keys) {
                    const float t0 = read(in, k).value_or(t), t1 = read(in, k + 1).value_or(t);
                    if (t1 > t0) {
                        x += (read(out, k + 1).value_or(x) - x) * std::clamp((t - t0) / (t1 - t0), 0.0f, 1.0f);
                    }
                }
                track.push_back(std::clamp(static_cast<int>(std::lround(x * 100.0f)), 0, 11));
            }
            break;
        }
    }
    return faces;
}

static const AnimSource& anim_source(const fs::path& path) {
    static std::mutex mutex;
    static std::map<fs::path, AnimSource> cache;
    std::scoped_lock lock(mutex);

    std::error_code ec;
    const auto mtime = fs::last_write_time(path, ec);
    const auto size = fs::file_size(path, ec);
    AnimSource& src = cache[path];
    if (src.anims && src.mtime == mtime && src.size == size) {
        return src;
    }

    if (src.anims) {
        unload_model_animations(src.anims, src.count);
    }
    src = AnimSource{};
    src.mtime = mtime;
    src.size = size;
    Model glb_model = load_model(path.string().c_str());
    if (glb_model.skeleton.bones) {
        for (int i = 0; i < glb_model.skeleton.boneCount; i++) {
            src.bone_names.emplace_back(glb_model.skeleton.bones[i].name);
        }
    }
    unload_model(glb_model);
    src.anims = load_model_animations(path.string().c_str(), &src.count);
    if (src.anims) {
        src.faces = read_glb_faces(path, src.anims, src.count);
    }
    return src;
}

static ModelAnimation* reindexed_animations(const Model& model, const AnimSource& src) {
    if (!src.anims || src.count <= 0) {
        return nullptr;
    }
    const bool reindex = model.skeleton.bones && !src.bone_names.empty();
    std::unordered_map<std::string, int> glb_bone_idx;
    for (int i = 0; i < std::ssize(src.bone_names); i++) {
        glb_bone_idx[src.bone_names[i]] = i;
    }

    auto* out = static_cast<ModelAnimation*>(std::calloc(src.count, sizeof(ModelAnimation)));
    if (!out) {
        return nullptr;
    }
    for (int a = 0; a < src.count; a++) {
        const ModelAnimation& in = src.anims[a];
        ModelAnimation& anim = out[a];
        std::memcpy(anim.name, in.name, sizeof(anim.name));
        anim.boneCount = in.boneCount;
        if (in.keyframeCount <= 0 || !in.keyframePoses) {
            continue;
        }

        const int n = reindex ? model.skeleton.boneCount : static_cast<int>(in.boneCount);
        anim.keyframePoses = static_cast<Transform**>(std::calloc(in.keyframeCount, sizeof(Transform*)));
        if (!anim.keyframePoses) {
            continue;
        }
        anim.keyframeCount = in.keyframeCount;
        anim.boneCount = n;
        for (int f = 0; f < in.keyframeCount; f++) {
            // calloc'd: a failed allocation leaves the rest null, which UnloadModelAnimations frees safely
            anim.keyframePoses[f] = static_cast<Transform*>(std::malloc(n * sizeof(Transform)));
            if (!anim.keyframePoses[f]) {
                break;
            }
            if (!reindex) {
                std::memcpy(anim.keyframePoses[f], in.keyframePoses[f], n * sizeof(Transform));
                continue;
            }
            for (int b = 0; b < n; b++) {
                auto it = glb_bone_idx.find(model.skeleton.bones[b].name);
                if (it != glb_bone_idx.end() && it->second < static_cast<int>(in.boneCount)) {
                    anim.keyframePoses[f][b] = in.keyframePoses[f][it->second];
                } else {
                    anim.keyframePoses[f][b] = model.skeleton.bindPose[b];
                }
            }
        }
    }
    return out;
}

static std::string name_lower(const char* s) {
    std::string r(s);
    for (char& c : r) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return r;
}

static std::unordered_map<std::string, int>
parse_glb_material_indices(const std::string& path, std::vector<int>& recolor_out, int& face_out,
                           std::vector<int>& additive_out, std::vector<int>& cutout_out, std::vector<int>& blend_out,
                           std::vector<int>& twosided_out) {
    std::unordered_map<std::string, int> result;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return result;
    }

    uint32_t magic = 0, version = 0, total_len = 0;
    if (fread(&magic, 4, 1, f) != 1 || fread(&version, 4, 1, f) != 1 || fread(&total_len, 4, 1, f) != 1) {
        fclose(f);
        return result;
    }

    if (magic != 0x46546C67u) {
        fclose(f);
        return result;
    }

    uint32_t chunk_len = 0, chunk_type = 0;
    if (fread(&chunk_len, 4, 1, f) != 1 || fread(&chunk_type, 4, 1, f) != 1) {
        fclose(f);
        return result;
    }

    if (chunk_type != 0x4E4F534Au) {
        fclose(f);
        return result;
    }

    if (chunk_len == 0 || total_len < 20u || chunk_len > total_len - 20u) {
        fclose(f);
        return result;
    }
    std::string json(chunk_len, '\0');
    const size_t read = fread(json.data(), 1, chunk_len, f);
    fclose(f);
    if (read != chunk_len) {
        return result;
    }

    rapidjson::Document doc;
    doc.Parse(json.data(), json.size());
    if (doc.HasParseError() || !doc.HasMember("materials")) {
        return result;
    }

    const auto& materials = doc["materials"];
    for (rapidjson::SizeType i = 0; i < materials.Size(); i++) {
        int raylib_idx = static_cast<int>(i) + 1;
        const char* mat_name_raw = nullptr;
        if (materials[i].HasMember("name") && materials[i]["name"].IsString()) {
            mat_name_raw = materials[i]["name"].GetString();
            result[mat_name_raw] = raylib_idx;
        }
        if (materials[i].HasMember("extras") && materials[i]["extras"].IsObject()) {
            const auto& extras = materials[i]["extras"];
            if (extras.HasMember("shaderType") && extras["shaderType"].IsString()) {
                std::string shader = extras["shaderType"].GetString();
                if (shader == "taikoEffectChangeColors") {
                    recolor_out.push_back(raylib_idx);
                } else if (shader == "taikoEffectFace" && face_out == -1) {
                    face_out = raylib_idx;
                }
            }
        }
        if (mat_name_raw) {
            std::string nl = name_lower(mat_name_raw);
            // chibidon's static face plane mirrors the player's face
            if (face_out == -1 && nl.starts_with("body_027000_02_")) {
                face_out = raylib_idx;
                std::erase(recolor_out, raylib_idx);
            }
            if (nl.contains("_aa_add")) {
                additive_out.push_back(raylib_idx);
            } else if (nl.contains("_color_s_cus_") && !nl.contains("_a_ab")) {
                cutout_out.push_back(raylib_idx); // _AT_ZERO_ / _AT_ONE_: alpha-tested, never blended
            }
            const bool cutout = nl.contains("_color_s_cus_") && !nl.contains("_a_ab");
            const bool gltf_blend = materials[i].HasMember("alphaMode") && materials[i]["alphaMode"].IsString() &&
                                    std::string(materials[i]["alphaMode"].GetString()) == "BLEND";
            // alpha-blended: _A_AB by name, or any other glTF BLEND material that is not an
            // alpha-tested one (e.g. a plain "lambert" glint sheet) -- drawn after the face
            if (nl.contains("_a_ab") || (gltf_blend && !cutout)) {
                blend_out.push_back(raylib_idx);
            }
            if (nl.contains("cullnone")) {
                twosided_out.push_back(raylib_idx);
            }
        }
    }
    return result;
}

static void normalize_face_mesh_size(Mesh& mesh, float target_size) {
    if (mesh.vertexCount == 0 || !mesh.vertices) {
        return;
    }
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f, minz = 1e9f, maxz = -1e9f;
    for (int v = 0; v < mesh.vertexCount; v++) {
        float x = mesh.vertices[v * 3 + 0], y = mesh.vertices[v * 3 + 1], z = mesh.vertices[v * 3 + 2];
        minx = std::min(minx, x);
        maxx = std::max(maxx, x);
        miny = std::min(miny, y);
        maxy = std::max(maxy, y);
        minz = std::min(minz, z);
        maxz = std::max(maxz, z);
    }
    float cx = (minx + maxx) / 2, cy = (miny + maxy) / 2, cz = (minz + maxz) / 2;
    float size = std::max(maxx - minx, maxy - miny);
    if (size <= 0.0001f) {
        return;
    }
    float factor = target_size / size;
    for (int v = 0; v < mesh.vertexCount; v++) {
        mesh.vertices[v * 3 + 0] = cx + (mesh.vertices[v * 3 + 0] - cx) * factor;
        mesh.vertices[v * 3 + 1] = cy + (mesh.vertices[v * 3 + 1] - cy) * factor;
        mesh.vertices[v * 3 + 2] = cz + (mesh.vertices[v * 3 + 2] - cz) * factor;
    }
}

void Chara3D::load_part(const fs::path& model_path, const fs::path& anim_path, bool normalize_face_scale) {
    Model model = load_model(model_path.string().c_str());
    // The vertex colours stay as shipped: the main shader does not tint by them, and the black
    // line pass reads its per-vertex thickness from the green channel.

    std::vector<int> recolor_indices, additive_indices, cutout_indices, blend_indices, twosided_indices;
    int face_material_index = -1;
    auto material_indices =
        parse_glb_material_indices(model_path.string(), recolor_indices, face_material_index, additive_indices,
                                   cutout_indices, blend_indices, twosided_indices);

    // Material indices come from the GLB's own JSON and are not guaranteed to line up
    // with what raylib actually imported (e.g. LoadModel failure, fewer materials).
    auto valid_material = [&](int idx) { return idx >= 0 && idx < model.materialCount; };
    if (!valid_material(face_material_index)) {
        face_material_index = -1;
    }
    auto filter_materials = [&](std::vector<int>& v) {
        std::erase_if(v, [&](int idx) { return !valid_material(idx); });
    };
    filter_materials(recolor_indices);
    filter_materials(additive_indices);
    filter_materials(cutout_indices);
    filter_materials(blend_indices);
    filter_materials(twosided_indices);
    for (auto it = material_indices.begin(); it != material_indices.end();) {
        if (!valid_material(it->second)) {
            it = material_indices.erase(it);
        } else {
            ++it;
        }
    }

    const bool mini_face = std::ranges::any_of(material_indices, [&](const auto& m) {
        return m.second == face_material_index && name_lower(m.first.c_str()).starts_with("body_027000_02_");
    });
    if (face_material_index != -1) {
        constexpr float COS_FACE_PLANE_SIZE = 0.137f;
        for (int m = 0; m < model.meshCount; m++) {
            if (model.meshMaterial[m] != face_material_index) {
                continue;
            }
            auto& mesh = model.meshes[m];
            float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
            for (int v = 0; v < mesh.vertexCount; v++) {
                minx = std::min(minx, mesh.vertices[v * 3]);
                maxx = std::max(maxx, mesh.vertices[v * 3]);
                miny = std::min(miny, mesh.vertices[v * 3 + 1]);
                maxy = std::max(maxy, mesh.vertices[v * 3 + 1]);
            }
            const float size = std::max(maxx - minx, maxy - miny);
            const float plate = normalize_face_scale || size > COS_FACE_PLANE_SIZE * 1.02f ? COS_FACE_PLANE_SIZE : size;
            normalize_face_mesh_size(mesh, mini_face ? plate : plate * 4.0f / 3.0f);
            update_mesh_buffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * static_cast<int>(sizeof(float)), 0);
            if (mesh.colors) {
                std::memset(mesh.colors, 255, static_cast<size_t>(mesh.vertexCount) * 4);
                update_mesh_buffer(mesh, 3, mesh.colors, mesh.vertexCount * 4, 0);
            }
        }
    }
#ifdef PLATFORM_ANDROID
    if (face_material_index != -1 && face_shader.id != 0) {
        model.materials[face_material_index].shader = face_shader;
    }
#endif
    std::erase(additive_indices, face_material_index);
    std::erase(blend_indices, face_material_index);
    for (int idx : additive_indices) {
        model.materials[idx].maps[material_map::DIFFUSE].color = {255, 255, 255, 255};
    }
    if (cutout_shader.id != 0) {
        for (int i = 0; i < model.materialCount; i++) {
            if (i == face_material_index) {
                continue;
            }
            model.materials[i].shader = cutout_shader;
        }
    }

    const AnimSource& anim_src = anim_source(anim_path);
    ModelAnimation* anims = reindexed_animations(model, anim_src);
    int anim_count = anims ? anim_src.count : 0;
    if (anim_faces.empty()) {
        anim_faces = anim_src.faces;
    }

    parts.push_back(model);
    part_material_indices.push_back(std::move(material_indices));
    part_recolor_indices.push_back(std::move(recolor_indices));
    part_additive_indices.push_back(std::move(additive_indices));
    part_cutout_indices.push_back(std::move(cutout_indices));
    part_blend_indices.push_back(std::move(blend_indices));
    part_twosided_indices.push_back(std::move(twosided_indices));
    part_face_material_index.push_back(face_material_index);
    part_anims.push_back(anims);
    part_anim_count.push_back(anim_count);
    find_rotors(static_cast<int>(parts.size()) - 1);
}

void Chara3D::find_rotors(int part_index) {
    const Model& model = parts[part_index];
    const ModelSkeleton& sk = model.skeleton;
    int bone = -1;
    for (int b = 0; b < static_cast<int>(sk.boneCount); b++) {
        if (std::string_view(sk.bones[b].name) == "swing_puro_null") {
            bone = b;
        }
    }
    if (bone < 0 || !sk.bindPose || sk.bones[bone].parent < 0) {
        return;
    }
    const Vec3 pivot = sk.bindPose[bone].translation;
    const float hub_y = sk.bindPose[sk.bones[bone].parent].translation.y;
    for (int m = 0; m < model.meshCount; m++) {
        const Mesh& mesh = model.meshes[m];
        if (!mesh.vertices) {
            continue;
        }
        std::vector<int> root(mesh.vertexCount);
        std::iota(root.begin(), root.end(), 0);
        auto find = [&](int v) {
            while (root[v] != v) {
                v = root[v] = root[root[v]];
            }
            return v;
        };
        for (int t = 0; t < mesh.triangleCount; t++) {
            int v[3];
            for (int k = 0; k < 3; k++) {
                v[k] = mesh.indices ? mesh.indices[t * 3 + k] : t * 3 + k;
            }
            root[find(v[1])] = find(v[0]);
            root[find(v[2])] = find(v[0]);
        }
        std::map<std::array<float, 3>, int> seam; // UV seams split a piece into co-located verts
        for (int v = 0; v < mesh.vertexCount; v++) {
            const float* p = &mesh.vertices[v * 3];
            const auto [it, added] = seam.try_emplace({p[0], p[1], p[2]}, v);
            if (!added) {
                root[find(v)] = find(it->second);
            }
        }
        std::vector<bool> below(mesh.vertexCount, false);
        for (int v = 0; v < mesh.vertexCount; v++) {
            if (mesh.vertices[v * 3 + 1] <= hub_y) {
                below[find(v)] = true;
            }
        }
        Rotor rotor{part_index, m, pivot.x, pivot.z, {}, {}};
        for (int v = 0; v < mesh.vertexCount; v++) {
            if (below[find(v)]) {
                continue;
            }
            rotor.verts.push_back(v);
            for (int k = 0; k < 3; k++) {
                rotor.rest.push_back(mesh.vertices[v * 3 + k]);
            }
            for (int k = 0; k < 3; k++) {
                rotor.rest.push_back(mesh.normals ? mesh.normals[v * 3 + k] : 0.0f);
            }
        }
        if (!rotor.verts.empty()) {
            rotors.push_back(std::move(rotor));
        }
    }
}

void Chara3D::spin_rotors(double current_ms) {
    if (rotors.empty()) {
        return;
    }
    constexpr double TURNS_PER_MS = 2.0 / 1000.0;
    const double angle = std::fmod(current_ms * TURNS_PER_MS, 1.0) * 2.0 * std::numbers::pi;
    const float c = static_cast<float>(std::cos(angle));
    const float s = static_cast<float>(std::sin(angle));
    for (const Rotor& rotor : rotors) {
        Mesh& mesh = parts[rotor.part].meshes[rotor.mesh];
        for (size_t i = 0; i < rotor.verts.size(); i++) {
            const float* r = &rotor.rest[i * 6];
            float* pos = &mesh.vertices[rotor.verts[i] * 3];
            const float dx = r[0] - rotor.pivot_x;
            const float dz = r[2] - rotor.pivot_z;
            pos[0] = rotor.pivot_x + dx * c + dz * s;
            pos[2] = rotor.pivot_z - dx * s + dz * c;
            if (mesh.normals) {
                float* n = &mesh.normals[rotor.verts[i] * 3];
                n[0] = r[3] * c + r[5] * s;
                n[2] = -r[3] * s + r[5] * c;
            }
        }
        const int bytes = mesh.vertexCount * 3 * static_cast<int>(sizeof(float));
        update_mesh_buffer(mesh, 0, mesh.vertices, bytes, 0);
        if (mesh.normals) {
            update_mesh_buffer(mesh, 2, mesh.normals, bytes, 0);
        }
    }
    applied_frame = -1;
    apply_pose();
}

static void init_shaders(Shader& outline_fxaa_shader, int& outline_fxaa_size_loc, int& outline_fxaa_thickness_loc,
                         Shader& null_shader, Shader& face_shader, Shader& cutout_shader, Shader& outline_shader,
                         bool& use_render_textures) {
    outline_fxaa_shader = load_shader("shader/pass.vs", "shader/outline_fxaa.fs");
    outline_fxaa_size_loc = get_shader_location(outline_fxaa_shader, "texSize");
    outline_fxaa_thickness_loc = get_shader_location(outline_fxaa_shader, "outlineThickness");
    float outline_thickness = 3.0f;
    set_shader_value(outline_fxaa_shader, outline_fxaa_thickness_loc, &outline_thickness, shader_uniform::FLOAT);

    const float tint_color[3] = {0.0f, 0.0f, 0.0f};
    const float tint_amount = 0.0f;
    set_shader_value(outline_fxaa_shader, get_shader_location(outline_fxaa_shader, "tintColor"), tint_color,
                     shader_uniform::VEC3);
    set_shader_value(outline_fxaa_shader, get_shader_location(outline_fxaa_shader, "tintAmount"), &tint_amount,
                     shader_uniform::FLOAT);

    null_shader = load_shader(nullptr, "shader/null.fs");
    face_shader = load_shader(nullptr, "shader/face.fs");
    cutout_shader = load_shader(nullptr, "shader/cutout.fs");
    outline_shader = load_shader("shader/outline.vs", "shader/outline.fs");

    if (outline_fxaa_shader.id == 0) {
        use_render_textures = false;
    }
}

Chara3D::Chara3D(std::string& model_name, bool mirror, bool use_skin_config) {
    init_shaders(outline_fxaa_shader, outline_fxaa_size_loc, outline_fxaa_thickness_loc, null_shader, face_shader,
                 cutout_shader, outline_shader, use_render_textures);
    this->mirror = mirror;
    Chara3DConfig cfg = use_skin_config ? tex.chara_3d_config : Chara3DConfig{};
    scale = cfg.scale;
    rot_x = cfg.rot_x;
    rot_y = cfg.rot_y;
    rot_z = cfg.rot_z;

    // Models has no inheritance mechanism of its own (unlike Graphics) — each asset
    // resolves against the child skin first, falling back to the parent's.
    fs::path model_path = resolve_skin_path(fs::path("Models/cos") / (model_name + ".glb"));
    fs::path anim_path = resolve_skin_path("Models/animations.glb");
    load_part(model_path, anim_path);

    load_face_textures(resolve_skin_path("Models/face/face_000000.png"));

    set_anim(anim_index);
}

Chara3D::Chara3D(std::string& head_name, std::string& body_name, bool mirror, bool use_skin_config) {
    init_shaders(outline_fxaa_shader, outline_fxaa_size_loc, outline_fxaa_thickness_loc, null_shader, face_shader,
                 cutout_shader, outline_shader, use_render_textures);
    this->mirror = mirror;
    Chara3DConfig cfg = use_skin_config ? tex.chara_3d_config : Chara3DConfig{};
    scale = cfg.scale;
    rot_x = cfg.rot_x;
    rot_y = cfg.rot_y;
    rot_z = cfg.rot_z;

    fs::path head_path = resolve_skin_path(fs::path("Models/head") / (head_name + ".glb"));
    fs::path body_path = resolve_skin_path(fs::path("Models/body") / (body_name + ".glb"));
    fs::path anim_path = resolve_skin_path("Models/animations.glb");
    load_part(body_path, anim_path);
    load_part(head_path, anim_path, true);

    load_face_textures(resolve_skin_path("Models/face/face_000000.png"));

    set_anim(anim_index);
}

Chara3D::~Chara3D() {
    for (size_t p = 0; p < parts.size(); p++) {
        if (part_face_material_index[p] != -1 && !face_textures.empty()) {
            parts[p].materials[part_face_material_index[p]].maps[material_map::DIFFUSE].texture.id = 0;
        }
        unload_model_animations(part_anims[p], part_anim_count[p]);
        unload_model(parts[p]);
    }
    unload_shader(null_shader);
    unload_shader(face_shader);
    unload_shader(cutout_shader);
    unload_shader(outline_fxaa_shader);
    if (scene_target.id != 0) {
        unload_render_texture(scene_target);
    }
    unload_shader(outline_shader);
    for (auto& tex : face_textures) {
        unload_texture(tex);
    }
    if (puchi_tex.id != 0) {
        unload_texture(puchi_tex);
        unload_texture(puchi_silhouette);
    }
}

void Chara3D::set_puchi(int index) {
    if (index == puchi_index) {
        return;
    }
    puchi_index = index;
    if (puchi_tex.id != 0) {
        unload_texture(puchi_tex);
        unload_texture(puchi_silhouette);
        puchi_tex = {};
        puchi_silhouette = {};
    }
    if (index <= 0) {
        return;
    }
    fs::path path = resolve_skin_path(fs::path("Models/acce") / (std::to_string(index) + ".png"));
    if (fs::exists(path)) {
        Image img = load_image(path.string().c_str());
        image_format(&img, pixel_format::RGBA8);
        puchi_tex = load_texture_from_image(img);
        auto* px = static_cast<unsigned char*>(img.data);
        for (int i = 0; i < img.width * img.height; i++) {
            px[4 * i] = px[4 * i + 1] = px[4 * i + 2] = 255;
        }
        puchi_silhouette = load_texture_from_image(img);
        unload_image(img);
        set_texture_filter(puchi_tex, texture_filter::BILINEAR);
        set_texture_filter(puchi_silhouette, texture_filter::BILINEAR);
    }
}

void Chara3D::set_texture(fs::path& texture_path, int part_index, int material_index) {
    Texture old = parts[part_index].materials[material_index].maps[material_map::DIFFUSE].texture;
    // Face material textures are owned by face_textures (shared across parts and unloaded
    // in the destructor); unloading here would double-free / use-after-free.
    bool is_face_material =
        part_face_material_index[part_index] != -1 && material_index == part_face_material_index[part_index];
    if (old.id != 0 && !is_face_material) {
        unload_texture(old);
    }
    Texture tex = load_texture_mipmapped(texture_path.string().c_str(), texture_filter::BILINEAR);
    int map_type = material_map::DIFFUSE;
    set_material_texture(&parts[part_index].materials[material_index], map_type, tex);
    render_dirty = true;
}

void Chara3D::set_body_texture(fs::path& texture_path) {
    for (size_t p = 0; p < parts.size(); p++) {
        auto it = part_material_indices[p].find("RGB_don_color_S_CUS_0x10000001_");
        if (it != part_material_indices[p].end()) {
            set_texture(texture_path, static_cast<int>(p), it->second);
            return;
        }
    }
}

void Chara3D::set_face_rim_texture(fs::path& texture_path) {
    for (size_t p = 0; p < parts.size(); p++) {
        auto it = part_material_indices[p].find("don_FACEHIP_color_S_CUS_0x10000001_");
        if (it != part_material_indices[p].end()) {
            set_texture(texture_path, static_cast<int>(p), it->second);
            return;
        }
    }
}

void Chara3D::load_face_textures(const fs::path& sheet_path) {
    if (!fs::exists(sheet_path)) {
        return;
    }
    Image sheet = load_image(sheet_path.string().c_str());
    // Frames are square and stacked vertically; derive the size from the
    // sheet width instead of assuming 128px, so higher-resolution skins
    // slice on the right boundaries (the UVs are normalized anyway).
    int frame_size = sheet.width;
    int frame_count = frame_size > 0 ? sheet.height / frame_size : 0;
    for (int f = 0; f < frame_count; f++) {
        Rect rect = {0, static_cast<float>(f * frame_size), static_cast<float>(frame_size),
                     static_cast<float>(frame_size)};
        Image frame_img = image_from_image(sheet, rect);
        face_textures.push_back(load_texture_from_image(frame_img));
        unload_image(frame_img);
    }
    unload_image(sheet);
    apply_face(std::max(current_face_index, 0));
}

void Chara3D::set_paint(int index) {
    if (index == paint_index) {
        return;
    }
    fs::path sheet = index > 0 ? resolve_skin_path(fs::path("Models/paint") / (std::to_string(index) + ".png"))
                               : resolve_skin_path("Models/face/face_000000.png");
    if (!fs::exists(sheet)) {
        return;
    }
    paint_index = index;
    for (auto& t : face_textures) {
        unload_texture(t);
    }
    face_textures.clear();
    load_face_textures(sheet);
}

void Chara3D::apply_anim_face() {
    const int ai = static_cast<int>(anim_index);
    if (ai < 0 || ai >= std::ssize(anim_faces) || anim_faces[ai].empty()) {
        return;
    }
    const auto& track = anim_faces[ai];
    const int face = track[std::min(anim_frame, static_cast<int>(track.size()) - 1)];
    if (face != current_face_index) {
        apply_face(face);
    }
}

void Chara3D::apply_face(int face_index) {
    if (face_index < 0 || face_index >= std::ssize(face_textures)) {
        return;
    }
    for (size_t p = 0; p < parts.size(); p++) {
        if (part_face_material_index[p] == -1) {
            continue;
        }
        parts[p].materials[part_face_material_index[p]].maps[material_map::DIFFUSE].texture = face_textures[face_index];
    }
    current_face_index = face_index;
    render_dirty = true;
}

static Texture recolor_texture(Image& source, Color body, Color face, Color rim) {
    Image img = image_copy(source);
    image_format(&img, pixel_format::RGBA8);

    auto* pixels = static_cast<unsigned char*>(img.data);
    int total = img.width * img.height;

    for (int i = 0; i < total; i++) {
        float r = pixels[i * 4 + 0] / 255.0f;
        float g = pixels[i * 4 + 1] / 255.0f;
        float b = pixels[i * 4 + 2] / 255.0f;

        float strongest = fmaxf(r, fmaxf(g, b));
        float weakest = fminf(r, fminf(g, b));
        if (strongest <= 0.05f || (strongest - weakest) <= 0.08f) {
            continue;
        }

        Color out;
        if (b > r && b >= g) {
            out = rim;
        } else if (g > r && g > b) {
            out = face;
        } else {
            out = body;
        }

        pixels[i * 4 + 0] = out.r;
        pixels[i * 4 + 1] = out.g;
        pixels[i * 4 + 2] = out.b;
    }

    Texture result = load_texture_from_image(img);
    unload_image(img);
    return result;
}

static void apply_don_colors(Model& model, int mat_idx, Color body, Color face, Color rim) {
    auto& map = model.materials[mat_idx].maps[material_map::DIFFUSE];
    Image img = load_image_from_texture(map.texture);
    if (!img.data) {
        return;
    }
    Texture new_tex = recolor_texture(img, body, face, rim);
    unload_image(img);
    if (new_tex.id == 0) {
        return;
    }
    unload_texture(map.texture);
    map.texture = new_tex;
}

void Chara3D::set_don_colors(Color body, Color face, Color rim) {
    for (size_t p = 0; p < parts.size(); p++) {
        for (int idx : part_recolor_indices[p]) {
            apply_don_colors(parts[p], idx, body, face, rim);
        }
    }
    render_dirty = true;
}

static constexpr int MIRROR_OFFSET = static_cast<int>(AnimIndex::DON_BALLOON_FAILURE_MIRROR);
static constexpr int EXTRA_START = static_cast<int>(AnimIndex::DON_ENTRY1P_COSCH_IN);
static constexpr int EXTRA_OFFSET = static_cast<int>(AnimIndex::DON_ENTRY1P_COSCH_IN_MIRROR) - EXTRA_START;

static int base_anim(AnimIndex idx) {
    const int i = static_cast<int>(idx);
    if (i >= EXTRA_START + EXTRA_OFFSET) {
        return i - EXTRA_OFFSET;
    }
    return i >= EXTRA_START ? i : i % MIRROR_OFFSET;
}

static int mirror_of(int base) {
    return base + (base >= EXTRA_START ? EXTRA_OFFSET : MIRROR_OFFSET);
}

static bool holds_last_frame(int base) {
    using A = AnimIndex;
    return base == static_cast<int>(A::DON_ENTRY1P_OUT) || base == static_cast<int>(A::DON_ENTRY2P_OUT) ||
           base == static_cast<int>(A::DON_SELECT1P_OUT) || base == static_cast<int>(A::DON_SELECT2P_OUT);
}

static bool is_kusu_pump(int base) {
    return base == static_cast<int>(AnimIndex::DON_KUSU1P_LOOP) || base == static_cast<int>(AnimIndex::DON_KUSU2P_LOOP);
}

static int two_player_of(int base) {
    using A = AnimIndex;
    switch (static_cast<A>(base)) {
    case A::DON_ENTRY1P_COSCH_IN:
    case A::DON_ENTRY1P_COSCH_OUT:
    case A::DON_ENTRY1P_OUT:
    case A::DON_ENTRY1P_SELECT:
    case A::DON_ENTRY1P_SELECT_LOOP:
        return base + static_cast<int>(A::DON_ENTRY2P_COSCH_IN) - static_cast<int>(A::DON_ENTRY1P_COSCH_IN);
    case A::DON_FUKKATU1P_SUCCESS:
        return static_cast<int>(A::DON_FUKKATU2P_SUCCESS);
    case A::DON_GENERAL1P_FULLGAGE:
    case A::DON_GENERAL1P_ITEM_IN:
    case A::DON_GENERAL1P_ITEM_LOOP:
    case A::DON_GENERAL1P_ITEM_OUT:
        return base + static_cast<int>(A::DON_GENERAL2P_FULLGAGE) - static_cast<int>(A::DON_GENERAL1P_FULLGAGE);
    case A::DON_KUSU1P_FAILURE:
    case A::DON_KUSU1P_IN:
    case A::DON_KUSU1P_LOOP:
    case A::DON_KUSU1P_NOBEAT:
    case A::DON_KUSU1P_SUCCESS01:
    case A::DON_KUSU1P_SUCCESS02:
        return base + static_cast<int>(A::DON_KUSU2P_FAILURE) - static_cast<int>(A::DON_KUSU1P_FAILURE);
    case A::DON_RESULT_FAILURE:
        return static_cast<int>(A::DON_RESULT2P_FAILURE);
    case A::DON_RESULT_FAILURE_LOOP:
        return static_cast<int>(A::DON_RESULT2P_FAILURE_LOOP);
    case A::DON_SELECT1P_GIN:
    case A::DON_SELECT1P_OUT:
        return base + static_cast<int>(A::DON_SELECT2P_GIN) - static_cast<int>(A::DON_SELECT1P_GIN);
    default:
        return -1;
    }
}

void Chara3D::set_anim(AnimIndex idx) {
    const AnimIndex requested = idx;
    int base = base_anim(idx);
    if (base == static_cast<int>(AnimIndex::DON_ENTRY_IN)) {
        set_anim(AnimIndex::DON_ENTRY_LOOP);
    } else if (base == static_cast<int>(AnimIndex::DON_FUKKATU_START)) {
        set_anim(AnimIndex::DON_FUKKATU_LOOP);
    }
    int i = base;
    if (mirror) {
        if (const int two_p = two_player_of(base); two_p >= 0) {
            i = base = two_p;
        } else {
            i = mirror_of(base);
        }
    }
    if (mirror && (base == static_cast<int>(AnimIndex::DON_MISS) || base == static_cast<int>(AnimIndex::DON_MISS6))) {
        i = static_cast<int>(AnimIndex::DON_MISS6_MIRROR) + static_cast<int>(AnimIndex::DON_MISS_MIRROR) - i;
    }
    idx = static_cast<AnimIndex>(i);
    int anim_count = part_anim_count.empty() ? 0 : part_anim_count[0];
    if (i >= anim_count && anim_count > 0 && base != static_cast<int>(AnimIndex::DON_NORMAL)) {
        set_anim(AnimIndex::DON_NORMAL);
        return;
    }
    if (i >= 0 && i < anim_count) {
        if (base == static_cast<int>(AnimIndex::DON_NORMAL) || base == static_cast<int>(AnimIndex::DON_SABI) ||
            base == static_cast<int>(AnimIndex::DON_BALLOON_NOBEAT) || base == static_cast<int>(AnimIndex::DON_MISS6) ||
            base == static_cast<int>(AnimIndex::DON_KUSU1P_NOBEAT) ||
            base == static_cast<int>(AnimIndex::DON_KUSU2P_NOBEAT) ||
            base == static_cast<int>(AnimIndex::DON_FUKKATU_FACE01) ||
            (get_anim_name(i).contains("loop") && !is_kusu_pump(base))) {
            is_looping = true;
        } else {
            if (is_looping) {
                prev_anim_idx = requested_anim; // not while chaining one-shots, or one would return to itself
            }
            is_looping = false;
        }
        anim_index = idx;
        requested_anim = requested;
        anim_frame = 0;
        holding = false;
        last_frame_ms = 0;
        render_dirty = true;
        apply_anim_face();
    }
}

std::string Chara3D::get_anim_name(int idx) {
    if (!part_anims.empty() && idx >= 0 && idx < part_anim_count[0]) {
        return part_anims[0][idx].name;
    }
    return "";
}

bool Chara3D::is_balloon_result() const {
    const int base = base_anim(anim_index);
    return base == static_cast<int>(AnimIndex::DON_BALLOON_SUCCESS) ||
           base == static_cast<int>(AnimIndex::DON_BALLOON_FAILURE);
}

void Chara3D::set_tint(Color color, float amount) {
    tint_color = color;
    tint_amount = amount;
    if (outline_fxaa_shader.id == 0) {
        return;
    }
    if (outline_fxaa_tint_color_loc < 0) {
        outline_fxaa_tint_color_loc = get_shader_location(outline_fxaa_shader, "tintColor");
        outline_fxaa_tint_amount_loc = get_shader_location(outline_fxaa_shader, "tintAmount");
    }
    float rgb[3] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
    set_shader_value(outline_fxaa_shader, outline_fxaa_tint_color_loc, rgb, shader_uniform::VEC3);
    set_shader_value(outline_fxaa_shader, outline_fxaa_tint_amount_loc, &amount, shader_uniform::FLOAT);
}

void Chara3D::apply_pose() {
    const int ai = static_cast<int>(anim_index);
    if (applied_anim == ai && applied_frame == anim_frame) {
        return;
    }
    for (size_t p = 0; p < parts.size(); p++) {
        if (ai < 0 || ai >= part_anim_count[p]) {
            continue;
        }
        update_model_animation(parts[p], part_anims[p][ai], anim_frame);
    }
    apply_anim_face();
    applied_anim = ai;
    applied_frame = anim_frame;
    render_dirty = true;
}

void Chara3D::set_rotation(float x, float y, float z) {
    if (rot_x == x && rot_y == y && rot_z == z) {
        return;
    }
    rot_x = x;
    rot_y = y;
    rot_z = z;
    render_dirty = true;
}

void Chara3D::set_mirror(bool value) {
    if (mirror == value) {
        return;
    }
    mirror = value;
    const int frame = anim_frame;
    const double frame_ms = last_frame_ms;
    set_anim(requested_anim);
    anim_frame = frame;
    last_frame_ms = frame_ms;
}

void Chara3D::use_default_pose(bool new_mirror) {
    const Chara3DConfig cfg{};
    scale = cfg.scale;
    rot_x = cfg.rot_x;
    rot_y = cfg.rot_y;
    rot_z = cfg.rot_z;
    mirror = new_mirror;
    set_anim(requested_anim);
    render_dirty = true;
}

void Chara3D::set_bpm(float bpm) {
    this->bpm = bpm;
}

void Chara3D::set_beat(double chart_beat) {
    beat_advanced = chart_beat > beat;
    beat = chart_beat;
}

int Chara3D::get_anim_count() const {
    return part_anim_count.empty() ? 0 : part_anim_count[0];
}

void Chara3D::update(double current_ms) {
    puchi_ms = current_ms;
    spin_rotors(current_ms);
    int anim_count = part_anim_count.empty() ? 0 : part_anim_count[0];
    if (anim_count > 0) {
        int ai = static_cast<int>(anim_index);
        const int kf = part_anims[0][ai].keyframeCount;
        if (bpm > 0.0f && kf > 0 && !holding) {
            double ms_per_beat = 60000.0 / bpm;
            const int base = base_anim(anim_index);
            const bool beat_synced = base < EXTRA_START || kf <= 2;
            if (!beat_synced) {
                ms_per_beat = (kf - 1) * 1000.0 / 60.0;
            } else if (base == static_cast<int>(AnimIndex::DON_NORMAL)) {
                ms_per_beat *= 4;
            } else if (base == static_cast<int>(AnimIndex::DON_NORM_LOOP)) {
                ms_per_beat *= 2;
            } else if (base == static_cast<int>(AnimIndex::DON_SABI)) || (base == static_cast<int>(AnimIndex::DON_SABI_START)) {
                ms_per_beat *= 8;
            }
            if (base == static_cast<int>(AnimIndex::DON_BALLOON_LOOP)) {
                ms_per_beat /= 2;
            }
            const int loop_frames = kf - 1;
            if (loop_frames <= 0) {
                if (current_ms - last_frame_ms >= ms_per_beat && !is_looping) {
                    last_frame_ms = current_ms;
                    set_anim(prev_anim_idx);
                    is_looping = true;
                }
            } else if (is_looping && beat_synced && beat_advanced) {
                const double beats_per_loop = ms_per_beat * bpm / 60000.0;
                double phase = std::fmod(beat, beats_per_loop);
                if (phase < 0.0) {
                    phase += beats_per_loop;
                }
                const int frame = std::min(static_cast<int>(phase / beats_per_loop * loop_frames), loop_frames - 1);
                last_frame_ms = current_ms;
                if (frame != anim_frame) {
                    anim_frame = frame;
                    apply_pose();
                }
            } else {
                const double ms_per_frame = ms_per_beat / loop_frames;
                int steps = static_cast<int>((current_ms - last_frame_ms) / ms_per_frame);
                if (steps > loop_frames * 2) {
                    steps = 1;
                    last_frame_ms = current_ms - ms_per_frame;
                }
                for (int i = 0; i < steps; ++i) {
                    last_frame_ms += ms_per_frame;
                    anim_frame = (anim_frame + 1) % loop_frames;
                    if (!is_looping && anim_frame == loop_frames - 1 && holds_last_frame(base)) {
                        anim_frame = loop_frames;
                        holding = true;
                        break;
                    }
                    if (!is_looping && anim_frame == loop_frames - 1) {
                        set_anim(prev_anim_idx);
                        is_looping = true;
                        break;
                    }
                }
                if (steps > 0) {
                    apply_pose();
                }
            }
        }
    }
}

void Chara3D::draw_outline(float x, float y, int rt_w, int rt_h, float full_h) {
    float y_angle = mirror ? -rot_y : rot_y;
    Mat4 rot = rotation_xyz(rot_x * deg2rad, y_angle * deg2rad, rot_z * deg2rad);
    const float draw_size = scale * draw_scale * tex.screen_scale;
    const Mat4 transform = matrix_multiply(
        rot, matrix_multiply(matrix_scale(draw_size, draw_size, draw_size), matrix_translate(x, y, 400.0f)));

    {
        const float thickness_px = 2.5f * draw_scale * full_h / 720.0f;
        float param[2] = {thickness_px, 0.3f};
        float size[2] = {static_cast<float>(rt_w), static_cast<float>(rt_h)};
        if (outline_param_loc < 0) {
            outline_param_loc = get_shader_location(outline_shader, "outlineParam");
        }
        if (outline_size_loc < 0) {
            outline_size_loc = get_shader_location(outline_shader, "screenSize");
        }
        set_shader_value(outline_shader, outline_param_loc, param, shader_uniform::VEC2);
        set_shader_value(outline_shader, outline_size_loc, size, shader_uniform::VEC2);
        set_cull_face(cull_face::FRONT);
        for (size_t p = 0; p < parts.size(); ++p) {
            const auto& part = parts[p];
            for (int mesh = 0; mesh < part.meshCount; ++mesh) {
                const int material_index = part.meshMaterial[mesh];
                Material material = part.materials[material_index];
                const bool is_face = material_index == part_face_material_index[p] && null_shader.id != 0;
                material.shader = is_face ? null_shader : outline_shader;
                draw_mesh(part.meshes[mesh], material, transform);
            }
        }
        set_cull_face(cull_face::BACK);
    }
}

void Chara3D::draw_3d(float x, float y) {
    float y_angle = mirror ? -rot_y : rot_y;
    Mat4 rot = rotation_xyz(rot_x * deg2rad, y_angle * deg2rad, rot_z * deg2rad);
    const float draw_size = scale * draw_scale * tex.screen_scale;
    const Mat4 transform = matrix_multiply(
        rot, matrix_multiply(matrix_scale(draw_size, draw_size, draw_size), matrix_translate(x, y, 400.0f)));
    for (size_t p = 0; p < parts.size(); p++) {
        draw_model_face_last(parts[p], part_face_material_index[p], part_blend_indices[p], part_twosided_indices[p],
                             transform);
    }
}

// Supersampling before the FXAA pass; rw/rh come from GetRenderWidth/Height.
static const float SUPERSAMPLE = platform_supersample();

// Backend bounds include the current pose, including meshes deformed only on the GPU.
bool Chara3D::screen_bounds(float x, float y, const Camera3D& cam3d, float zoom, int rw, int rh, float& x0, float& y0,
                            float& x1, float& y1) {
    float y_angle = mirror ? -rot_y : rot_y;
    Mat4 rot = rotation_xyz(rot_x * deg2rad, y_angle * deg2rad, rot_z * deg2rad);
    const float s = scale * draw_scale * tex.screen_scale;
    Mat4 transform = matrix_multiply(rot, matrix_multiply(matrix_scale(s, s, s), matrix_translate(x, y, 400.0f)));
    x0 = y0 = std::numeric_limits<float>::max();
    x1 = y1 = std::numeric_limits<float>::lowest();
    bool any = false;
    for (const auto& part : parts) {
        for (int m = 0; m < part.meshCount; m++) {
            const Mesh& mesh = part.meshes[m];
            Vec3 lower, upper;
            if (!get_mesh_bounds(mesh, lower, upper)) {
                continue;
            }
            // Transforming the local box's corners conservatively bounds every posed vertex.
            for (int corner = 0; corner < 8; ++corner) {
                Vec3 w = vector3_transform({(corner & 1) ? upper.x : lower.x, (corner & 2) ? upper.y : lower.y,
                                            (corner & 4) ? upper.z : lower.z},
                                           transform);
                const float px = (w.x - cam3d.position.x) * zoom + rw * 0.5f;
                const float py = (w.y - cam3d.position.y) * zoom + rh * 0.5f;
                x0 = std::min(x0, px);
                x1 = std::max(x1, px);
                y0 = std::min(y0, py);
                y1 = std::max(y1, py);
                any = true;
            }
        }
    }
    return any;
}

void Chara3D::draw(float x, float y, float scale_mul) {
    if (tex.options[SCO::DISABLE_CHARA_3D]) {
        return;
    }
    if (scale_mul != draw_scale) {
        draw_scale = scale_mul;
        render_dirty = true;
        if (outline_fxaa_shader.id != 0) {
            const float outline_thickness = 3.0f * draw_scale;
            set_shader_value(outline_fxaa_shader, outline_fxaa_thickness_loc, &outline_thickness,
                             shader_uniform::FLOAT);
        }
    }
    draw_body(x, y);
    draw_puchi(x, y);
}

void Chara3D::draw_puchi(float x, float y) {
    if (puchi_tex.id == 0) {
        return;
    }

    constexpr double FRAME_MS = 850.0;
    constexpr float PUCHI_X = 90.0f;
    constexpr float PUCHI_Y = -120.0f;
    constexpr float PUCHI_SIZE = 150.0f;
    constexpr float PUCHI_BOB = 6.0f;
    const float s = tex.screen_scale * draw_scale;
    const float fw = static_cast<float>(puchi_tex.width);
    const float fh = static_cast<float>(puchi_tex.height) / 2.0f;
    const int frame = static_cast<int>(puchi_ms / FRAME_MS) % 2;
    const float bob =
        std::sin(static_cast<float>(puchi_ms / (FRAME_MS * 2.0) * 2.0 * std::numbers::pi)) * PUCHI_BOB * s;
    const float size = PUCHI_SIZE * s;
    const float px = x + (mirror ? PUCHI_X : -PUCHI_X) * s - size / 2.0f;
    const float py = y + PUCHI_Y * s - size / 2.0f + bob;
    const Rect src{0, frame * fh, mirror ? -fw : fw, fh};
    draw_texture_pro(puchi_tex, src, {px, py, size, size}, {0, 0}, 0.0f, Color::WHITE);
    if (tint_amount > 0.0f) {
        Color overlay = tint_color;
        overlay.a = static_cast<unsigned char>(std::clamp(tint_amount, 0.0f, 1.0f) * 255.0f);
        draw_texture_pro(puchi_silhouette, src, {px, py, size, size}, {0, 0}, 0.0f, overlay);
    }
    // Matches outline_fxaa's formula: tint_color adds on top of tint_amount's darkening.
    if (tint_color.r > 0 || tint_color.g > 0 || tint_color.b > 0) {
        begin_blend_mode(blend_mode::ADDITIVE);
        draw_texture_pro(puchi_silhouette, src, {px, py, size, size}, {0, 0}, 0.0f, tint_color);
        end_blend_mode();
    }
}

void Chara3D::draw_body(float x, float y) {

    int rw = get_render_width();
    int rh = get_render_height();
    Camera2D cam2d = compute_camera2d(tex.screen_width, tex.screen_height);

    // A rotated camera (and no render textures) keep the full-screen path
    if (!use_render_textures || cam2d.rotation != 0.0f) {
        draw_full_target(x, y, rw, rh, cam2d);
        return;
    }

    Camera3D cam3d = camera2d_to_3d(cam2d);
    const float zoom = std::max(cam2d.zoom, 0.0001f);
    if (full_target || x != last_draw_x || y != last_draw_y || zoom != last_zoom || cam3d.position.x != last_cam_x ||
        cam3d.position.y != last_cam_y || rw != last_rw || rh != last_rh) {
        last_draw_x = x;
        last_draw_y = y;
        last_zoom = zoom;
        last_cam_x = cam3d.position.x;
        last_cam_y = cam3d.position.y;
        last_rw = rw;
        last_rh = rh;
        render_dirty = true;
    }

    if (render_dirty) {
        float bx0, by0, bx1, by1;
        if (!screen_bounds(x, y, cam3d, zoom, rw, rh, bx0, by0, bx1, by1)) {
            draw_full_target(x, y, rw, rh, cam2d);
            return;
        }
        // room for the outline hull (2.5 px at 720 lines, scaled by the vertex colour) and the
        // FXAA pass's 3-texel dilation, plus a safety pixel
        const float margin = 2.0f * (2.5f * rh / 720.0f) + 4.0f;
        const int need_w = static_cast<int>(std::ceil((bx1 - bx0) + 2.0f * margin));
        const int need_h = static_cast<int>(std::ceil((by1 - by0) + 2.0f * margin));
        // grow-only target, in 128-texel steps, so a changing pose rarely reallocates it
        auto round_up = [](int v) { return std::max(128, (v + 127) / 128 * 128); };
        const int aw = round_up(static_cast<int>(need_w * SUPERSAMPLE));
        const int ah = round_up(static_cast<int>(need_h * SUPERSAMPLE));
        if (scene_target.id == 0 || full_target || aw > scene_target_w || ah > scene_target_h) {
            const int nw = full_target ? aw : std::max(aw, scene_target_w);
            const int nh = full_target ? ah : std::max(ah, scene_target_h);
            if (scene_target.id != 0) {
                unload_render_texture(scene_target);
            }
            scene_target = load_render_texture(nw, nh);
            full_target = false;
            if (scene_target.id == 0) {
                spdlog::warn("Chara3D: render texture unavailable, using direct render");
                use_render_textures = false;
                draw_full_target(x, y, rw, rh, cam2d);
                return;
            }
            set_texture_filter(scene_target.texture, texture_filter::BILINEAR);
            scene_target_w = nw;
            scene_target_h = nh;
            float ts[2] = {static_cast<float>(nw), static_cast<float>(nh)};
            set_shader_value(outline_fxaa_shader, outline_fxaa_size_loc, ts, shader_uniform::VEC2);
        }
        // the target's area, centred on the model, on whole render pixels (the supersampled
        // grid then lines up with the full-screen one)
        const int region_w = static_cast<int>(scene_target_w / SUPERSAMPLE);
        const int region_h = static_cast<int>(scene_target_h / SUPERSAMPLE);
        region_x = static_cast<int>(std::floor((bx0 + bx1) * 0.5f - region_w * 0.5f));
        region_y = static_cast<int>(std::floor((by0 + by1) * 0.5f - region_h * 0.5f));

        // the full-screen camera narrowed to that rectangle: same centre offset and pixel scale
        Camera3D rc = cam3d;
        const float wcx = cam3d.position.x + (region_x + region_w * 0.5f - rw * 0.5f) / zoom;
        const float wcy = cam3d.position.y + (region_y + region_h * 0.5f - rh * 0.5f) / zoom;
        rc.position.x = rc.target.x = wcx;
        rc.position.y = rc.target.y = wcy;
        rc.fovy = region_h / zoom;

        end_mode_2d();
        end_blend_mode();
        render_dirty = false;
        begin_texture_mode(scene_target);
        clear_background(Color::BLANK);
        begin_blend_mode(blend_mode::CUSTOM_SEPARATE); // not BLEND_ALPHA: it blends dst alpha as a*a+dst*(1-a),
                                                       // making opaque pixels under a translucent sheet see-through
        begin_mode_3d(rc);
        draw_3d(x, y);
        draw_outline(x, y, scene_target_w, scene_target_h, rh * SUPERSAMPLE);
        end_mode_3d();
        end_blend_mode();
        end_texture_mode();
    } else {
        end_mode_2d();
        end_blend_mode();
    }

    {
        const float region_w = scene_target_w / SUPERSAMPLE;
        const float region_h = scene_target_h / SUPERSAMPLE;
        begin_shader_mode(outline_fxaa_shader);
        draw_texture_pro(scene_target.texture,
                         {0, 0, static_cast<float>(scene_target_w), -static_cast<float>(scene_target_h)},
                         {static_cast<float>(region_x), static_cast<float>(region_y), region_w, region_h}, {0, 0}, 0.0f,
                         Color::WHITE);
        end_shader_mode();
    }

    begin_blend_mode(blend_mode::CUSTOM_SEPARATE);
    begin_mode_2d(cam2d);
}

// The whole screen at 2x: kept for a rotated camera and as the fallback.
void Chara3D::draw_full_target(float x, float y, int rw, int rh, const Camera2D& cam2d) {
    int ssw = static_cast<int>(rw * SUPERSAMPLE);
    int ssh = static_cast<int>(rh * SUPERSAMPLE);

    if (scene_target.id == 0 || !full_target || scene_target_w != ssw || scene_target_h != ssh) {
        if (scene_target.id != 0) {
            unload_render_texture(scene_target);
        }
        scene_target = load_render_texture(ssw, ssh);
        if (scene_target.id == 0) {
            spdlog::warn("Chara3D: render texture unavailable, using direct render");
            use_render_textures = false;
        } else {
            set_texture_filter(scene_target.texture, texture_filter::BILINEAR);
        }
        scene_target_w = ssw;
        scene_target_h = ssh;
        full_target = true;
        float ts[2] = {static_cast<float>(ssw), static_cast<float>(ssh)};
        set_shader_value(outline_fxaa_shader, outline_fxaa_size_loc, ts, shader_uniform::VEC2);
        render_dirty = true;
    }

    if (!use_render_textures) {
        Camera3D cam3d = camera2d_to_3d(cam2d);
        end_mode_2d();
        end_blend_mode();
        begin_mode_3d(cam3d);
        draw_3d(x, y);
        end_mode_3d();
        begin_blend_mode(blend_mode::CUSTOM_SEPARATE);
        begin_mode_2d(cam2d);
        return;
    }

    if (x != last_draw_x || y != last_draw_y) {
        last_draw_x = x;
        last_draw_y = y;
        render_dirty = true;
    }

    end_mode_2d();
    end_blend_mode();

    if (render_dirty) {
        render_dirty = false;
        Camera3D cam3d = camera2d_to_3d(cam2d);

        begin_texture_mode(scene_target);
        clear_background(Color::BLANK);
        begin_blend_mode(blend_mode::CUSTOM_SEPARATE);
        begin_mode_3d(cam3d);
        draw_3d(x, y);
        draw_outline(x, y, ssw, ssh, ssh);
        end_mode_3d();
        end_blend_mode();
        end_texture_mode();
    }

    {
        begin_shader_mode(outline_fxaa_shader);
        draw_texture_pro(scene_target.texture, {0, 0, static_cast<float>(ssw), -static_cast<float>(ssh)},
                         {0, 0, static_cast<float>(rw), static_cast<float>(rh)}, {0, 0}, 0.0f, Color::WHITE);
        end_shader_mode();
    }

    begin_blend_mode(blend_mode::CUSTOM_SEPARATE);
    begin_mode_2d(cam2d);
}

std::unique_ptr<Chara3D> make_chara_from_player_data(const PlayerData* pd, bool mirror, bool use_skin_config) {
    if (pd && !pd->chara_is_costume) {
        std::string head_name = std::to_string(pd->chara_head_index);
        std::string body_name = std::to_string(pd->chara_body_index);
        auto chara = std::make_unique<Chara3D>(head_name, body_name, mirror, use_skin_config);
        chara->set_puchi(pd->chara_acce_index);
        chara->set_paint(pd->chara_paint_index);
        return chara;
    }
    std::string costume_name = pd ? std::to_string(pd->chara_cos_index) : "0";
    auto chara = std::make_unique<Chara3D>(costume_name, mirror, use_skin_config);
    if (pd) {
        chara->set_puchi(pd->chara_acce_index);
        chara->set_paint(pd->chara_paint_index);
    }
    return chara;
}
