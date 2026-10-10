#include "chara_3d.h"
#include "../../libs/animation.h"
#include "../../libs/camera_utils.h"
#include "../../libs/global_data.h"
#include "../../libs/scores.h"
#include "../../libs/filesystem.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <rapidjson/document.h>
namespace ray {
#include <raymath.h>
}
extern "C" { void rlSetCullFace(int mode); void rlEnableBackfaceCulling(void); void rlDisableBackfaceCulling(void); }
static constexpr int RL_CULL_FACE_FRONT = 0;
static constexpr int RL_CULL_FACE_BACK  = 1;

static void draw_model_face_last(ray::Model& model, int face_material_index, const std::vector<int>& blend_materials,
                                 const std::vector<int>& twosided_materials, ray::Vector3 position, float scale) {
    ray::Matrix matTransform = ray::MatrixMultiply(ray::MatrixScale(scale, scale, scale),
                                                     ray::MatrixTranslate(position.x, position.y, position.z));
    ray::Matrix transform = ray::MatrixMultiply(model.transform, matTransform);
    auto is_blend = [&](int mat) { return std::find(blend_materials.begin(), blend_materials.end(), mat) != blend_materials.end(); };
    // _CULLNONE materials are drawn two-sided, as the name asks.
    auto draw = [&](int i) {
        const int mat = model.meshMaterial[i];
        const bool two_sided = std::find(twosided_materials.begin(), twosided_materials.end(), mat) != twosided_materials.end();
        if (two_sided) rlDisableBackfaceCulling();
        ray::DrawMesh(model.meshes[i], model.materials[mat], transform);
        if (two_sided) rlEnableBackfaceCulling();
    };

    // 1. opaque and alpha-tested meshes
    for (int i = 0; i < model.meshCount; i++) {
        const int mat = model.meshMaterial[i];
        if (mat == face_material_index || is_blend(mat)) continue;
        draw(i);
    }
    // 2. the face plane
    if (face_material_index != -1) {
        for (int i = 0; i < model.meshCount; i++) {
            if (model.meshMaterial[i] == face_material_index)
                ray::DrawMesh(model.meshes[i], model.materials[model.meshMaterial[i]], transform);
        }
    }
    // 3. alpha-blended sheets (_A_AB / glTF BLEND: front hair, plates, glints) last. Drawn
    //    before the face, their fully transparent texels still wrote depth and occluded the face
    //    plane and the drum head behind them, so the face came out as the black hull. They keep
    //    writing depth among themselves, as before, so a plate in front of a hair sheet stays on top.
    for (int i = 0; i < model.meshCount; i++) {
        const int mat = model.meshMaterial[i];
        if (mat != face_material_index && is_blend(mat))
            draw(i);
    }
}

static ray::Matrix rotation_xyz(float ax, float ay, float az) {
    float cx = cosf(-ax), sx = sinf(-ax);
    float cy = cosf(-ay), sy = sinf(-ay);
    float cz = cosf(-az), sz = sinf(-az);
    ray::Matrix r = {};
    r.m0 = cz*cy;  r.m1 = (cz*sy*sx) - (sz*cx);  r.m2 = (cz*sy*cx) + (sz*sx);
    r.m4 = sz*cy;  r.m5 = (sz*sy*sx) + (cz*cx);   r.m6 = (sz*sy*cx) - (cz*sx);
    r.m8 = -sy;    r.m9 = cy*sx;                   r.m10 = cy*cx;
    r.m15 = 1.0f;
    return r;
}

// animations.glb is a few MB and every part of every Don used to parse it twice (LoadModel for
// its bone names, LoadModelAnimations for the poses): about 185 ms per part on every screen with
// a Don. It is parsed once per file (again if the file changes, e.g. after a skin update) and each
// part gets its own copy with the poses reordered to its skeleton.
struct AnimSource {
    fs::file_time_type mtime;
    std::uintmax_t size = 0;
    std::vector<std::string> bone_names;  // the glb's skeleton; empty if it has none
    ray::ModelAnimation* anims = nullptr;
    int count = 0;
};

static const AnimSource& anim_source(const fs::path& path) {
    static std::mutex mutex;
    static std::map<fs::path, AnimSource> cache;
    std::lock_guard<std::mutex> lock(mutex);

    std::error_code ec;
    const auto mtime = fs::last_write_time(path, ec);
    const auto size = fs::file_size(path, ec);
    AnimSource& src = cache[path];
    if (src.anims && src.mtime == mtime && src.size == size) return src;

    if (src.anims) ray::UnloadModelAnimations(src.anims, src.count);
    src = AnimSource{};
    src.mtime = mtime;
    src.size = size;
    ray::Model glb_model = ray::LoadModel(path.string().c_str());
    if (glb_model.skeleton.bones)
        for (int i = 0; i < glb_model.skeleton.boneCount; i++)
            src.bone_names.emplace_back(glb_model.skeleton.bones[i].name);
    ray::UnloadModel(glb_model);
    src.anims = ray::LoadModelAnimations(path.string().c_str(), &src.count);
    return src;
}

// A copy of `src`'s animations for `model`: the poses reordered to the model's bones, bones the glb
// does not animate held at the model's bind pose (copied as they are when either side has no
// skeleton). Freed with UnloadModelAnimations.
static ray::ModelAnimation* reindexed_animations(const ray::Model& model, const AnimSource& src) {
    if (!src.anims || src.count <= 0) return nullptr;
    const bool reindex = model.skeleton.bones && !src.bone_names.empty();
    std::unordered_map<std::string, int> glb_bone_idx;
    for (int i = 0; i < (int)src.bone_names.size(); i++)
        glb_bone_idx[src.bone_names[i]] = i;

    auto* out = (ray::ModelAnimation*)std::calloc(src.count, sizeof(ray::ModelAnimation));
    if (!out) return nullptr;
    for (int a = 0; a < src.count; a++) {
        const ray::ModelAnimation& in = src.anims[a];
        ray::ModelAnimation& anim = out[a];
        std::memcpy(anim.name, in.name, sizeof(anim.name));
        anim.boneCount = in.boneCount;
        if (in.keyframeCount <= 0 || !in.keyframePoses) continue;

        const int n = reindex ? model.skeleton.boneCount : (int)in.boneCount;
        anim.keyframePoses = (ray::ModelAnimPose*)std::calloc(in.keyframeCount, sizeof(ray::ModelAnimPose));
        if (!anim.keyframePoses) continue;
        anim.keyframeCount = in.keyframeCount;
        anim.boneCount = n;
        for (int f = 0; f < in.keyframeCount; f++) {
            // calloc'd: a failed allocation leaves the rest null, which UnloadModelAnimations frees safely
            anim.keyframePoses[f] = (ray::Transform*)std::malloc(n * sizeof(ray::Transform));
            if (!anim.keyframePoses[f]) break;
            if (!reindex) {
                std::memcpy(anim.keyframePoses[f], in.keyframePoses[f], n * sizeof(ray::Transform));
                continue;
            }
            for (int b = 0; b < n; b++) {
                auto it = glb_bone_idx.find(model.skeleton.bones[b].name);
                if (it != glb_bone_idx.end() && it->second < (int)in.boneCount)
                    anim.keyframePoses[f][b] = in.keyframePoses[f][it->second];
                else
                    anim.keyframePoses[f][b] = model.skeleton.bindPose[b];
            }
        }
    }
    return out;
}

static std::string name_lower(const char* s) {
    std::string r(s);
    for (char& c : r) c = (char)tolower((unsigned char)c);
    return r;
}

static std::unordered_map<std::string, int> parse_glb_material_indices(
        const std::string& path, std::vector<int>& recolor_out, int& face_out,
        std::vector<int>& additive_out, std::vector<int>& cutout_out, std::vector<int>& blend_out,
        std::vector<int>& twosided_out) {
    std::unordered_map<std::string, int> result;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return result;

    uint32_t magic = 0, version = 0, total_len = 0;
    if (fread(&magic, 4, 1, f) != 1 || fread(&version, 4, 1, f) != 1 ||
        fread(&total_len, 4, 1, f) != 1) { fclose(f); return result; }

    if (magic != 0x46546C67u) { fclose(f); return result; }

    uint32_t chunk_len = 0, chunk_type = 0;
    if (fread(&chunk_len, 4, 1, f) != 1 || fread(&chunk_type, 4, 1, f) != 1) {
        fclose(f);
        return result;
    }

    if (chunk_type != 0x4E4F534Au) { fclose(f); return result; }

    if (chunk_len == 0 || total_len < 20u || chunk_len > total_len - 20u) { fclose(f); return result; }
    std::string json(chunk_len, '\0');
    const size_t read = fread(json.data(), 1, chunk_len, f);
    fclose(f);
    if (read != chunk_len) return result;

    rapidjson::Document doc;
    doc.Parse(json.data(), json.size());
    if (doc.HasParseError() || !doc.HasMember("materials")) return result;

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
                if (shader == "taikoEffectChangeColors")
                    recolor_out.push_back(raylib_idx);
                else if (shader == "taikoEffectFace" && face_out == -1)
                    face_out = raylib_idx;
            }
        }
        if (mat_name_raw) {
            std::string nl = name_lower(mat_name_raw);
            if (nl.find("_aa_add") != std::string::npos)
                additive_out.push_back(raylib_idx);
            else if (nl.find("_color_s_cus_") != std::string::npos &&
                     nl.find("_a_ab") == std::string::npos)
                cutout_out.push_back(raylib_idx);   // _AT_ZERO_ / _AT_ONE_: alpha-tested, never blended
            const bool cutout = nl.find("_color_s_cus_") != std::string::npos && nl.find("_a_ab") == std::string::npos;
            const bool gltf_blend = materials[i].HasMember("alphaMode") && materials[i]["alphaMode"].IsString() &&
                                    std::string(materials[i]["alphaMode"].GetString()) == "BLEND";
            // alpha-blended: _A_AB by name, or any other glTF BLEND material that is not an
            // alpha-tested one (e.g. a plain "lambert" glint sheet) -- drawn after the face
            if (nl.find("_a_ab") != std::string::npos || (gltf_blend && !cutout))
                blend_out.push_back(raylib_idx);
            if (nl.find("cullnone") != std::string::npos)
                twosided_out.push_back(raylib_idx);
        }
    }
    return result;
}

static void normalize_face_mesh_size(ray::Mesh& mesh, float target_size) {
    if (mesh.vertexCount == 0 || !mesh.vertices) return;
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f, minz = 1e9f, maxz = -1e9f;
    for (int v = 0; v < mesh.vertexCount; v++) {
        float x = mesh.vertices[v * 3 + 0], y = mesh.vertices[v * 3 + 1], z = mesh.vertices[v * 3 + 2];
        minx = std::min(minx, x); maxx = std::max(maxx, x);
        miny = std::min(miny, y); maxy = std::max(maxy, y);
        minz = std::min(minz, z); maxz = std::max(maxz, z);
    }
    float cx = (minx + maxx) / 2, cy = (miny + maxy) / 2, cz = (minz + maxz) / 2;
    float size = std::max(maxx - minx, maxy - miny);
    if (size <= 0.0001f) return;
    float factor = target_size / size;
    for (int v = 0; v < mesh.vertexCount; v++) {
        mesh.vertices[v * 3 + 0] = cx + (mesh.vertices[v * 3 + 0] - cx) * factor;
        mesh.vertices[v * 3 + 1] = cy + (mesh.vertices[v * 3 + 1] - cy) * factor;
        mesh.vertices[v * 3 + 2] = cz + (mesh.vertices[v * 3 + 2] - cz) * factor;
    }
}

void Chara3D::load_part(const fs::path& model_path, const fs::path& anim_path, bool normalize_face_scale) {
    ray::Model model = ray::LoadModel(model_path.string().c_str());
    // The vertex colours stay as shipped: the main shader does not tint by them, and the black
    // line pass reads its per-vertex thickness from the green channel.

    std::vector<int> recolor_indices, additive_indices, cutout_indices, blend_indices, twosided_indices;
    int face_material_index = -1;
    auto material_indices = parse_glb_material_indices(model_path.string(), recolor_indices, face_material_index, additive_indices, cutout_indices, blend_indices, twosided_indices);

    // Material indices come from the GLB's own JSON and are not guaranteed to line up
    // with what raylib actually imported (e.g. LoadModel failure, fewer materials).
    auto valid_material = [&](int idx) { return idx >= 0 && idx < model.materialCount; };
    if (!valid_material(face_material_index)) face_material_index = -1;
    auto filter_materials = [&](std::vector<int>& v) {
        v.erase(std::remove_if(v.begin(), v.end(), [&](int idx) { return !valid_material(idx); }), v.end());
    };
    filter_materials(recolor_indices);
    filter_materials(additive_indices);
    filter_materials(cutout_indices);
    filter_materials(blend_indices);
    filter_materials(twosided_indices);
    for (auto it = material_indices.begin(); it != material_indices.end(); ) {
        if (!valid_material(it->second)) it = material_indices.erase(it);
        else ++it;
    }

    if (face_material_index != -1) {
        // head/body parts always get the standard 0.137 plate; a costume keeps its own plate
        // (some are deliberately small) but never a larger one -- cos 30 (鏡もち) and cos 9
        // ship a 0.18 plate that overflows the drum head.
        constexpr float COS_FACE_PLANE_SIZE = 0.137f;
        for (int m = 0; m < model.meshCount; m++) {
            if (model.meshMaterial[m] != face_material_index) continue;
            auto& mesh = model.meshes[m];
            float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
            for (int v = 0; v < mesh.vertexCount; v++) {
                minx = std::min(minx, mesh.vertices[v * 3]); maxx = std::max(maxx, mesh.vertices[v * 3]);
                miny = std::min(miny, mesh.vertices[v * 3 + 1]); maxy = std::max(maxy, mesh.vertices[v * 3 + 1]);
            }
            const float size = std::max(maxx - minx, maxy - miny);
            if (normalize_face_scale || size > COS_FACE_PLANE_SIZE * 1.02f) {
                normalize_face_mesh_size(mesh, COS_FACE_PLANE_SIZE);
                ray::UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * (int)sizeof(float), 0);
            }
        }
    }
#ifdef PLATFORM_ANDROID
    if (face_material_index != -1 && face_shader.id != 0)
        model.materials[face_material_index].shader = face_shader;
#endif
    additive_indices.erase(
        std::remove(additive_indices.begin(), additive_indices.end(), face_material_index),
        additive_indices.end());
    blend_indices.erase(
        std::remove(blend_indices.begin(), blend_indices.end(), face_material_index),
        blend_indices.end());
    for (int idx : additive_indices)
        model.materials[idx].maps[ray::MATERIAL_MAP_DIFFUSE].color = {255, 255, 255, 255};
    // Alpha-tested materials: the textures are binary alpha with black RGB under the
    // transparent texels (fins, tentacles, hair tips). Forcing alpha to 255 painted all of
    // that solid black; blending them would need back-to-front sorting. An alpha test gives
    // the cabinet's cutout look with plain depth writes.
    if (cutout_shader.id != 0)
        for (int i = 0; i < model.materialCount; i++) {
            if (i == face_material_index) continue;
            model.materials[i].shader = cutout_shader;
        }

    const AnimSource& anim_src = anim_source(anim_path);
    ray::ModelAnimation* anims = reindexed_animations(model, anim_src);
    int anim_count = anims ? anim_src.count : 0;

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
}

static void init_shaders(ray::Shader& outline_fxaa_shader, int& outline_fxaa_size_loc, int& outline_fxaa_thickness_loc,
                          ray::Shader& null_shader, ray::Shader& face_shader, ray::Shader& cutout_shader,
                          ray::Shader& outline_shader, bool& use_render_textures) {
    outline_fxaa_shader = load_shader("shader/pass.vs", "shader/outline_fxaa.fs");
    outline_fxaa_size_loc = ray::GetShaderLocation(outline_fxaa_shader, "texSize");
    outline_fxaa_thickness_loc = ray::GetShaderLocation(outline_fxaa_shader, "outlineThickness");
    float outline_thickness = 3.0f;
    ray::SetShaderValue(outline_fxaa_shader, outline_fxaa_thickness_loc, &outline_thickness, ray::SHADER_UNIFORM_FLOAT);

    null_shader    = load_shader(nullptr, "shader/null.fs");
    face_shader    = load_shader(nullptr, "shader/face.fs");
    cutout_shader  = load_shader(nullptr, "shader/cutout.fs");
    outline_shader = load_shader("shader/outline.vs", "shader/outline.fs");

    if (outline_fxaa_shader.id == 0)
        use_render_textures = false;
}

Chara3D::Chara3D(std::string& model_name, bool mirror, bool use_skin_config) {
    init_shaders(outline_fxaa_shader, outline_fxaa_size_loc, outline_fxaa_thickness_loc,
                 null_shader, face_shader, cutout_shader, outline_shader, use_render_textures);
    this->mirror = mirror;
    Chara3DConfig cfg = use_skin_config ? tex.chara_3d_config : Chara3DConfig{};
    scale = cfg.scale;
    rot_x = cfg.rot_x;
    rot_y = cfg.rot_y;
    rot_z = cfg.rot_z;

    // Models has no inheritance mechanism of its own (unlike Graphics) — each asset
    // resolves against the child skin first, falling back to the parent's.
    fs::path model_path = resolve_skin_path(fs::path("Models/cos") / (model_name + ".glb"));
    fs::path anim_path  = resolve_skin_path("Models/animations.glb");
    load_part(model_path, anim_path);

    fs::path face_dir = resolve_skin_path("Models/face");
    load_face_textures(face_dir);

    fs::path skin_anim_path = resolve_skin_path("Graphics/global/animation.json");
    load_face_anims(skin_anim_path);

    set_anim(anim_index);
}

Chara3D::Chara3D(std::string& head_name, std::string& body_name, bool mirror, bool use_skin_config) {
    init_shaders(outline_fxaa_shader, outline_fxaa_size_loc, outline_fxaa_thickness_loc,
                 null_shader, face_shader, cutout_shader, outline_shader, use_render_textures);
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

    fs::path face_dir = resolve_skin_path("Models/face");
    load_face_textures(face_dir);

    fs::path skin_anim_path = resolve_skin_path("Graphics/global/animation.json");
    load_face_anims(skin_anim_path);

    set_anim(anim_index);
}

Chara3D::~Chara3D() {
    for (size_t p = 0; p < parts.size(); p++) {
        if (part_face_material_index[p] != -1 && !face_textures.empty())
            parts[p].materials[part_face_material_index[p]].maps[ray::MATERIAL_MAP_DIFFUSE].texture.id = 0;
        ray::UnloadModelAnimations(part_anims[p], part_anim_count[p]);
        ray::UnloadModel(parts[p]);
    }
    ray::UnloadShader(null_shader);
    ray::UnloadShader(face_shader);
    ray::UnloadShader(cutout_shader);
    ray::UnloadShader(outline_fxaa_shader);
    if (scene_target.id != 0) ray::UnloadRenderTexture(scene_target);
    ray::UnloadShader(outline_shader);
    for (auto& tex : face_textures)
        ray::UnloadTexture(tex);
}

void Chara3D::set_texture(fs::path& texture_path, int part_index, int material_index) {
    ray::Texture2D old = parts[part_index].materials[material_index].maps[ray::MATERIAL_MAP_DIFFUSE].texture;
    // Face material textures are owned by face_textures (shared across parts and unloaded
    // in the destructor); unloading here would double-free / use-after-free.
    bool is_face_material = part_face_material_index[part_index] != -1 &&
                             material_index == part_face_material_index[part_index];
    if (old.id != 0 && !is_face_material) ray::UnloadTexture(old);
    ray::Texture tex = ray::LoadTexture(texture_path.string().c_str());
    ray::GenTextureMipmaps(&tex);
    ray::SetTextureFilter(tex, ray::TEXTURE_FILTER_BILINEAR);
    int map_type = ray::MATERIAL_MAP_DIFFUSE;
    ray::SetMaterialTexture(&parts[part_index].materials[material_index], map_type, tex);
    render_dirty = true;
}

void Chara3D::set_body_texture(fs::path& texture_path) {
    for (size_t p = 0; p < parts.size(); p++) {
        auto it = part_material_indices[p].find("RGB_don_color_S_CUS_0x10000001_");
        if (it != part_material_indices[p].end()) { set_texture(texture_path, (int)p, it->second); return; }
    }
}

void Chara3D::set_face_rim_texture(fs::path& texture_path) {
    for (size_t p = 0; p < parts.size(); p++) {
        auto it = part_material_indices[p].find("don_FACEHIP_color_S_CUS_0x10000001_");
        if (it != part_material_indices[p].end()) { set_texture(texture_path, (int)p, it->second); return; }
    }
}

void Chara3D::load_face_textures(fs::path& face_dir) {
    if (!fs::exists(face_dir)) return;
    std::vector<fs::path> paths;
    for (auto& e : fs::directory_iterator(face_dir)) {
        if (e.path().extension() == ".png")
            paths.push_back(e.path());
    }
    if (paths.empty()) return;
    std::sort(paths.begin(), paths.end());
    ray::Image sheet = ray::LoadImage(paths[0].string().c_str());
    // Frames are square and stacked vertically; derive the size from the
    // sheet width instead of assuming 128px, so higher-resolution skins
    // slice on the right boundaries (the UVs are normalized anyway).
    int frame_size = sheet.width;
    int frame_count = frame_size > 0 ? sheet.height / frame_size : 0;
    for (int f = 0; f < frame_count; f++) {
        ray::Rectangle rect = {0, (float)(f * frame_size), (float)frame_size, (float)frame_size};
        ray::Image frame_img = ray::ImageFromImage(sheet, rect);
        face_textures.push_back(ray::LoadTextureFromImage(frame_img));
        ray::UnloadImage(frame_img);
    }
    ray::UnloadImage(sheet);
    apply_face(0);
}

void Chara3D::load_face_anims(fs::path& anim_path) {
    if (!fs::exists(anim_path)) return;
    std::ifstream f(anim_path.string());
    if (!f.is_open()) return;
    std::string json_str((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());
    AnimationParser parser;
    face_anims = parser.parseAnimationsFromString(json_str);
}

void Chara3D::apply_face(int face_index) {
    if (face_index < 0 || face_index >= (int)face_textures.size()) return;
    for (size_t p = 0; p < parts.size(); p++) {
        if (part_face_material_index[p] == -1) continue;
        parts[p].materials[part_face_material_index[p]].maps[ray::MATERIAL_MAP_DIFFUSE].texture =
            face_textures[face_index];
    }
    current_face_index = face_index;
    render_dirty = true;
}

static ray::Texture2D recolor_texture(ray::Image& source,
                                       ray::Color body, ray::Color face, ray::Color rim) {
    ray::Image img = ray::ImageCopy(source);
    ray::ImageFormat(&img, ray::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    unsigned char* pixels = (unsigned char*)img.data;
    int total = img.width * img.height;

    for (int i = 0; i < total; i++) {
        float r = pixels[i * 4 + 0] / 255.0f;
        float g = pixels[i * 4 + 1] / 255.0f;
        float b = pixels[i * 4 + 2] / 255.0f;

        float strongest = fmaxf(r, fmaxf(g, b));
        float weakest   = fminf(r, fminf(g, b));
        if (strongest <= 0.05f || (strongest - weakest) <= 0.08f) continue;

        ray::Color out;
        if (b > r && b >= g)     out = rim;
        else if (g > r && g > b) out = face;
        else                      out = body;

        pixels[i * 4 + 0] = out.r;
        pixels[i * 4 + 1] = out.g;
        pixels[i * 4 + 2] = out.b;
    }

    ray::Texture2D result = ray::LoadTextureFromImage(img);
    ray::UnloadImage(img);
    return result;
}

static void apply_don_colors(ray::Model& model, int mat_idx,
                              ray::Color body, ray::Color face, ray::Color rim) {
    auto& map = model.materials[mat_idx].maps[ray::MATERIAL_MAP_DIFFUSE];
    ray::Image img = ray::LoadImageFromTexture(map.texture);
    if (!img.data) return;
    ray::Texture2D new_tex = recolor_texture(img, body, face, rim);
    ray::UnloadImage(img);
    if (new_tex.id == 0) return;
    ray::UnloadTexture(map.texture);
    map.texture = new_tex;
}

void Chara3D::set_don_colors(ray::Color body, ray::Color face, ray::Color rim) {
    for (size_t p = 0; p < parts.size(); p++)
        for (int idx : part_recolor_indices[p])
            apply_don_colors(parts[p], idx, body, face, rim);
    render_dirty = true;
}

static constexpr int FACE_ANIM_IDS[] = {
    13, 14, 15, 16, 65, 17, 22, 19, 30, 29, 23, 24, 25, 26,
    40, 40, 42, 43, 44, 45, 46, 58, 59, 62, 60, 18,
    13, 14, 15, 16, 64, 17, 22, 21, 36, 35, 23, 24, 25, 26,
    40, 40, 42, 43, 44, 45, 46, 58, 59, 62, 61, 20,
};

static constexpr int MIRROR_OFFSET = (int)AnimIndex::DON_BALLOON_FAILURE_MIRROR;
static int base_anim(AnimIndex idx) { return static_cast<int>(idx) % MIRROR_OFFSET; }

void Chara3D::set_anim(AnimIndex idx) {
    const int base = base_anim(idx);
    if (base == (int)AnimIndex::DON_ENTRY_IN) set_anim(AnimIndex::DON_ENTRY_LOOP);
    else if (base == (int)AnimIndex::DON_FUKKATU_START) set_anim(AnimIndex::DON_FUKKATU_LOOP);
    int i = base + (mirror ? MIRROR_OFFSET : 0);
    if (mirror && (base == (int)AnimIndex::DON_MISS || base == (int)AnimIndex::DON_MISS6))
        i = (int)AnimIndex::DON_MISS6_MIRROR + (int)AnimIndex::DON_MISS_MIRROR - i;
    idx = static_cast<AnimIndex>(i);
    int anim_count = part_anim_count.empty() ? 0 : part_anim_count[0];
    if (i >= 0 && i < anim_count) {
        if (base == (int)AnimIndex::DON_NORMAL || base == (int)AnimIndex::DON_SABI ||
            base == (int)AnimIndex::DON_BALLOON_NOBEAT || base == (int)AnimIndex::DON_MISS6 ||
            get_anim_name(i).find("loop") != std::string::npos) {
            is_looping = true;
        } else {
            if (is_looping) prev_anim_idx = anim_index;  // not while chaining one-shots, or one would return to itself
            is_looping = false;
        }
        anim_index = idx;
        anim_frame = 0;
        last_frame_ms = 0;
        render_dirty = true;
    }

    if (i >= 0 && i < (int)(sizeof(FACE_ANIM_IDS) / sizeof(FACE_ANIM_IDS[0]))) {
        auto it = face_anims.find(FACE_ANIM_IDS[i]);
        if (it != face_anims.end()) {
            current_face_anim = it->second.get();
            current_face_anim->reset();
            current_face_anim->start();
            apply_face((int)current_face_anim->attribute);
        }
    }
}

std::string Chara3D::get_anim_name(int idx) {
    if (!part_anims.empty() && idx >= 0 && idx < part_anim_count[0]) {
        return part_anims[0][idx].name;
    }
    return "";
}

void Chara3D::set_tint(ray::Color color, float amount) {
    if (outline_fxaa_shader.id == 0) return;
    if (outline_fxaa_tint_color_loc < 0) {
        outline_fxaa_tint_color_loc = ray::GetShaderLocation(outline_fxaa_shader, "tintColor");
        outline_fxaa_tint_amount_loc = ray::GetShaderLocation(outline_fxaa_shader, "tintAmount");
    }
    float rgb[3] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
    ray::SetShaderValue(outline_fxaa_shader, outline_fxaa_tint_color_loc, rgb, ray::SHADER_UNIFORM_VEC3);
    ray::SetShaderValue(outline_fxaa_shader, outline_fxaa_tint_amount_loc, &amount, ray::SHADER_UNIFORM_FLOAT);
}

void Chara3D::set_bpm(float bpm) {
    this->bpm = bpm;
}

int Chara3D::get_anim_count() const {
    return part_anim_count.empty() ? 0 : part_anim_count[0];
}

void Chara3D::update(double current_ms) {
    int anim_count = part_anim_count.empty() ? 0 : part_anim_count[0];
    if (anim_count > 0) {
        int ai = static_cast<int>(anim_index);
        const int kf = part_anims[0][ai].keyframeCount;
        if (bpm > 0.0f && kf > 0) {
            double ms_per_beat = 60000.0 / bpm;
            const int base = base_anim(anim_index);
            if (base == (int)AnimIndex::DON_NORMAL) ms_per_beat *= 3;
            else if (base == (int)AnimIndex::DON_SABI) ms_per_beat *= 6;
            if (base == (int)AnimIndex::DON_BALLOON_LOOP) ms_per_beat /= 2;
            double ms_per_frame = ms_per_beat / kf;
            if (current_ms - last_frame_ms >= ms_per_frame) {
                int loop_frames = kf - 1;
                last_frame_ms = current_ms;

                if (loop_frames <= 0) {
                    if (!is_looping) {
                        set_anim(prev_anim_idx);
                        is_looping = true;
                    }
                } else {
                    anim_frame = (anim_frame + 1) % loop_frames;
                    // UpdateModelAnimation CPU-skins and uploads position/normal
                    // buffers to the GPU itself; no manual UpdateMeshBuffer needed
                    for (size_t p = 0; p < parts.size(); p++) {
                        if (ai >= part_anim_count[p]) continue;
                        ray::UpdateModelAnimation(parts[p], part_anims[p][ai], anim_frame);
                    }
                    render_dirty = true;

                    if (!is_looping && anim_frame == loop_frames - 1) {
                        set_anim(prev_anim_idx);
                        is_looping = true;
                    }
                }
            }
        }
    }

    if (current_face_anim) {
        current_face_anim->update(current_ms);
        int new_face = (int)current_face_anim->attribute;
        if (new_face != current_face_index)
            apply_face(new_face);
    }
}

void Chara3D::draw_outline(float x, float y, int rt_w, int rt_h, float full_h) {
    std::vector<std::vector<ray::Shader>> saved(parts.size());
    for (size_t p = 0; p < parts.size(); p++) {
        saved[p].resize(parts[p].materialCount);
        for (int i = 0; i < parts[p].materialCount; i++) {
            saved[p][i] = parts[p].materials[i].shader;
            bool is_face = (part_face_material_index[p] != -1 && i == part_face_material_index[p] && null_shader.id != 0);
            // additive glows (_AA_ADD) and alpha-blended sheets (_A_AB) have no silhouette to
            // outline; a hull under them is a black box seen through the transparent texels
            parts[p].materials[i].shader = is_face ? null_shader : outline_shader;
        }
    }

    std::vector<ray::Matrix> saved_transform(parts.size());
    float y_angle = mirror ? -rot_y : rot_y;
    ray::Matrix rot = rotation_xyz(rot_x * DEG2RAD, y_angle * DEG2RAD, rot_z * DEG2RAD);
    for (size_t p = 0; p < parts.size(); p++) {
        saved_transform[p] = parts[p].transform;
        parts[p].transform = rot;
    }

    {
        const float thickness_px = 2.5f * full_h / 720.0f;
        float param[2] = {thickness_px, 0.3f};
        float size[2]  = {(float)rt_w, (float)rt_h};
        if (outline_param_loc < 0) outline_param_loc = ray::GetShaderLocation(outline_shader, "outlineParam");
        if (outline_size_loc < 0)  outline_size_loc  = ray::GetShaderLocation(outline_shader, "screenSize");
        ray::SetShaderValue(outline_shader, outline_param_loc, param, ray::SHADER_UNIFORM_VEC2);
        ray::SetShaderValue(outline_shader, outline_size_loc, size, ray::SHADER_UNIFORM_VEC2);
        const float draw_size = scale * draw_scale * tex.screen_scale;

        rlSetCullFace(RL_CULL_FACE_FRONT);
        for (auto& part : parts)
            ray::DrawModel(part, {x, y, 400.0f}, draw_size, ray::WHITE);
        rlSetCullFace(RL_CULL_FACE_BACK);
    }

    for (size_t p = 0; p < parts.size(); p++) {
        parts[p].transform = saved_transform[p];
        for (int i = 0; i < parts[p].materialCount; i++)
            parts[p].materials[i].shader = saved[p][i];
    }
}

void Chara3D::draw_3d(float x, float y) {
    std::vector<ray::Matrix> saved(parts.size());
    float y_angle = mirror ? -rot_y : rot_y;
    ray::Matrix rot = rotation_xyz(rot_x * DEG2RAD, y_angle * DEG2RAD, rot_z * DEG2RAD);
    for (size_t p = 0; p < parts.size(); p++) {
        saved[p] = parts[p].transform;
        parts[p].transform = rot;
    }
    for (size_t p = 0; p < parts.size(); p++)
        draw_model_face_last(parts[p], part_face_material_index[p], part_blend_indices[p], part_twosided_indices[p], {x, y, 400.0f}, scale * draw_scale * tex.screen_scale);
    for (size_t p = 0; p < parts.size(); p++)
        parts[p].transform = saved[p];
}

// Render at 2x and downscale on blit: bilinear-filtered supersampling ahead of the
// FXAA pass, since raylib render textures have no MSAA path that stays GLES/Android-safe.
static constexpr float SUPERSAMPLE = 2.0f;

// Screen rectangle (render pixels) of the posed model: the CPU-skinned vertices (animVertices,
// see UpdateModelAnimation) through the same transform draw_3d uses, then the orthographic
// camera of camera2d_to_3d (camera rotation 0).
bool Chara3D::screen_bounds(float x, float y, const ray::Camera3D& cam3d, float zoom, int rw, int rh,
                            float& x0, float& y0, float& x1, float& y1) {
    float y_angle = mirror ? -rot_y : rot_y;
    ray::Matrix rot = rotation_xyz(rot_x * DEG2RAD, y_angle * DEG2RAD, rot_z * DEG2RAD);
    const float s = scale * draw_scale * tex.screen_scale;
    ray::Matrix transform = ray::MatrixMultiply(rot, ray::MatrixMultiply(ray::MatrixScale(s, s, s),
                                                                         ray::MatrixTranslate(x, y, 400.0f)));
    x0 = y0 = std::numeric_limits<float>::max();
    x1 = y1 = std::numeric_limits<float>::lowest();
    bool any = false;
    for (const auto& part : parts) {
        for (int m = 0; m < part.meshCount; m++) {
            const ray::Mesh& mesh = part.meshes[m];
            const float* v = mesh.animVertices ? mesh.animVertices : mesh.vertices;
            if (!v) continue;
            for (int i = 0; i < mesh.vertexCount; i++) {
                ray::Vector3 w = ray::Vector3Transform({v[3 * i], v[3 * i + 1], v[3 * i + 2]}, transform);
                const float px = (w.x - cam3d.position.x) * zoom + rw * 0.5f;
                const float py = (w.y - cam3d.position.y) * zoom + rh * 0.5f;
                x0 = std::min(x0, px); x1 = std::max(x1, px);
                y0 = std::min(y0, py); y1 = std::max(y1, py);
                any = true;
            }
        }
    }
    return any;
}

void Chara3D::draw(float x, float y, float scale_mul) {
    if (tex.options[SCO::DISABLE_CHARA_3D]) return;

    int rw = ray::GetRenderWidth();
    int rh = ray::GetRenderHeight();
    ray::Camera2D cam2d = compute_camera2d(tex.screen_width, tex.screen_height);

    // A rotated camera (and no render textures) keep the full-screen path
    if (!use_render_textures || cam2d.rotation != 0.0f) {
        draw_full_target(x, y, rw, rh, cam2d);
        return;
    }

    ray::Camera3D cam3d = camera2d_to_3d(cam2d);
    const float zoom = std::max(cam2d.zoom, 0.0001f);
    if (full_target || x != last_draw_x || y != last_draw_y || zoom != last_zoom ||
        cam3d.position.x != last_cam_x || cam3d.position.y != last_cam_y || rw != last_rw || rh != last_rh) {
        last_draw_x = x; last_draw_y = y; last_zoom = zoom;
        last_cam_x = cam3d.position.x; last_cam_y = cam3d.position.y; last_rw = rw; last_rh = rh;
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
        const float margin = 2.0f * (2.5f * (float)rh / 720.0f) + 4.0f;
        const int need_w = (int)std::ceil((bx1 - bx0) + 2.0f * margin);
        const int need_h = (int)std::ceil((by1 - by0) + 2.0f * margin);
        // grow-only target, in 128-texel steps, so a changing pose rarely reallocates it
        auto round_up = [](int v) { return std::max(128, (v + 127) / 128 * 128); };
        const int aw = round_up((int)(need_w * SUPERSAMPLE));
        const int ah = round_up((int)(need_h * SUPERSAMPLE));
        if (scene_target.id == 0 || full_target || aw > scene_target_w || ah > scene_target_h) {
            const int nw = full_target ? aw : std::max(aw, scene_target_w);
            const int nh = full_target ? ah : std::max(ah, scene_target_h);
            if (scene_target.id != 0) ray::UnloadRenderTexture(scene_target);
            scene_target = ray::LoadRenderTexture(nw, nh);
            full_target = false;
            if (scene_target.id == 0) {
                spdlog::warn("Chara3D: render texture unavailable, using direct render");
                use_render_textures = false;
                draw_full_target(x, y, rw, rh, cam2d);
                return;
            }
            ray::SetTextureFilter(scene_target.texture, ray::TEXTURE_FILTER_BILINEAR);
            scene_target_w = nw;
            scene_target_h = nh;
            float ts[2] = {(float)nw, (float)nh};
            ray::SetShaderValue(outline_fxaa_shader, outline_fxaa_size_loc, ts, ray::SHADER_UNIFORM_VEC2);
        }
        // the target's area, centred on the model, on whole render pixels (the supersampled
        // grid then lines up with the full-screen one)
        const int region_w = (int)(scene_target_w / SUPERSAMPLE);
        const int region_h = (int)(scene_target_h / SUPERSAMPLE);
        region_x = (int)std::floor((bx0 + bx1) * 0.5f - region_w * 0.5f);
        region_y = (int)std::floor((by0 + by1) * 0.5f - region_h * 0.5f);

        // the full-screen camera narrowed to that rectangle: same centre offset and pixel scale
        ray::Camera3D rc = cam3d;
        const float wcx = cam3d.position.x + (region_x + region_w * 0.5f - rw * 0.5f) / zoom;
        const float wcy = cam3d.position.y + (region_y + region_h * 0.5f - rh * 0.5f) / zoom;
        rc.position.x = rc.target.x = wcx;
        rc.position.y = rc.target.y = wcy;
        rc.fovy = region_h / zoom;

        ray::EndMode2D();
        ray::EndBlendMode();
        render_dirty = false;
        ray::BeginTextureMode(scene_target);
        ray::ClearBackground(ray::BLANK);
        ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE); // not BLEND_ALPHA: it blends dst alpha as a*a+dst*(1-a), making opaque pixels under a translucent sheet see-through
        ray::BeginMode3D(rc);
        draw_3d(x, y);
        draw_outline(x, y, scene_target_w, scene_target_h, rh * SUPERSAMPLE);
        ray::EndMode3D();
        ray::EndBlendMode();
        ray::EndTextureMode();
    } else {
        ray::EndMode2D();
        ray::EndBlendMode();
    }

    {
        const float region_w = scene_target_w / SUPERSAMPLE;
        const float region_h = scene_target_h / SUPERSAMPLE;
        ray::BeginShaderMode(outline_fxaa_shader);
        ray::DrawTexturePro(scene_target.texture,
            {0, 0, (float)scene_target_w, -(float)scene_target_h},
            {(float)region_x, (float)region_y, region_w, region_h},
            {0, 0}, 0.0f, ray::WHITE);
        ray::EndShaderMode();
    }

    ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE);
    ray::BeginMode2D(cam2d);
}

// The whole screen at 2x: kept for a rotated camera and as the fallback.
void Chara3D::draw_full_target(float x, float y, int rw, int rh, const ray::Camera2D& cam2d) {
    int ssw = (int)((float)rw * SUPERSAMPLE);
    int ssh = (int)((float)rh * SUPERSAMPLE);

    if (scene_target.id == 0 || !full_target || scene_target_w != ssw || scene_target_h != ssh) {
        if (scene_target.id != 0) ray::UnloadRenderTexture(scene_target);
        scene_target   = ray::LoadRenderTexture(ssw, ssh);
        if (scene_target.id == 0) {
            spdlog::warn("Chara3D: render texture unavailable, using direct render");
            use_render_textures = false;
        } else {
            ray::SetTextureFilter(scene_target.texture, ray::TEXTURE_FILTER_BILINEAR);
        }
        scene_target_w = ssw;
        scene_target_h = ssh;
        full_target = true;
        float ts[2] = {(float)ssw, (float)ssh};
        ray::SetShaderValue(outline_fxaa_shader, outline_fxaa_size_loc, ts, ray::SHADER_UNIFORM_VEC2);
        render_dirty = true;
    }

    if (!use_render_textures) {
        ray::Camera3D cam3d = camera2d_to_3d(cam2d);
        ray::EndMode2D();
        ray::EndBlendMode();
        ray::BeginMode3D(cam3d);
        draw_3d(x, y);
        ray::EndMode3D();
        ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE);
        ray::BeginMode2D(cam2d);
        return;
    }

    if (x != last_draw_x || y != last_draw_y) {
        last_draw_x = x;
        last_draw_y = y;
        render_dirty = true;
    }

    ray::EndMode2D();
    ray::EndBlendMode();

    if (render_dirty) {
        render_dirty = false;
        ray::Camera3D cam3d = camera2d_to_3d(cam2d);

        ray::BeginTextureMode(scene_target);
        ray::ClearBackground(ray::BLANK);
        ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE);
        ray::BeginMode3D(cam3d);
        draw_3d(x, y);
        draw_outline(x, y, ssw, ssh, (float)ssh);
        ray::EndMode3D();
        ray::EndBlendMode();
        ray::EndTextureMode();
    }

    {
        ray::BeginShaderMode(outline_fxaa_shader);
        ray::DrawTexturePro(scene_target.texture,
            {0, 0, (float)ssw, -(float)ssh},
            {0, 0, (float)rw, (float)rh},
            {0, 0}, 0.0f, ray::WHITE);
        ray::EndShaderMode();
    }

    ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE);
    ray::BeginMode2D(cam2d);
}

std::unique_ptr<Chara3D> make_chara_from_player_data(const PlayerData* pd, bool mirror, bool use_skin_config) {
    if (pd && !pd->chara_is_costume) {
        std::string head_name = std::to_string(pd->chara_head_index);
        std::string body_name = std::to_string(pd->chara_body_index);
        return std::make_unique<Chara3D>(head_name, body_name, mirror, use_skin_config);
    }
    std::string costume_name = pd ? std::to_string(pd->chara_cos_index) : "0";
    return std::make_unique<Chara3D>(costume_name, mirror, use_skin_config);
}
