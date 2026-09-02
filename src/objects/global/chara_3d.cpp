#include "chara_3d.h"
#include "../../libs/animation.h"
#include "../../libs/camera_utils.h"
#include "../../libs/global_data.h"
#include "../../libs/scores.h"
#include "../../libs/filesystem.h"
#include <fstream>
#include <rapidjson/document.h>
namespace ray {
#include <raymath.h>
}
extern "C" { void rlSetCullFace(int mode); }
static constexpr int RL_CULL_FACE_FRONT = 0;
static constexpr int RL_CULL_FACE_BACK  = 1;

static void draw_model_face_last(ray::Model& model, int face_material_index, ray::Vector3 position, float scale) {
    ray::Matrix matTransform = ray::MatrixMultiply(ray::MatrixScale(scale, scale, scale),
                                                     ray::MatrixTranslate(position.x, position.y, position.z));
    ray::Matrix transform = ray::MatrixMultiply(model.transform, matTransform);

    for (int i = 0; i < model.meshCount; i++) {
        if (model.meshMaterial[i] == face_material_index) continue;
        ray::DrawMesh(model.meshes[i], model.materials[model.meshMaterial[i]], transform);
    }
    if (face_material_index != -1) {
        for (int i = 0; i < model.meshCount; i++) {
            if (model.meshMaterial[i] == face_material_index)
                ray::DrawMesh(model.meshes[i], model.materials[model.meshMaterial[i]], transform);
        }
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

static void reindex_animations(ray::Model& model, ray::Model& glb_model,
                               ray::ModelAnimation* anims, int anim_count) {
    std::unordered_map<std::string, int> glb_bone_idx;
    for (int i = 0; i < glb_model.skeleton.boneCount; i++)
        glb_bone_idx[glb_model.skeleton.bones[i].name] = i;

    int n = model.skeleton.boneCount;

    for (int a = 0; a < anim_count; a++) {
        auto& anim = anims[a];
        ray::ModelAnimPose* new_poses =
            (ray::ModelAnimPose*)std::malloc(anim.keyframeCount * sizeof(ray::ModelAnimPose));

        for (int f = 0; f < anim.keyframeCount; f++) {
            new_poses[f] = (ray::Transform*)std::malloc(n * sizeof(ray::Transform));
            for (int b = 0; b < n; b++) {
                auto it = glb_bone_idx.find(model.skeleton.bones[b].name);
                if (it != glb_bone_idx.end() && it->second < anim.boneCount)
                    new_poses[f][b] = anim.keyframePoses[f][it->second];
                else
                    new_poses[f][b] = model.skeleton.bindPose[b];
            }
            std::free(anim.keyframePoses[f]);
        }
        std::free(anim.keyframePoses);
        anim.keyframePoses = new_poses;
        anim.boneCount = n;
    }
}

static std::string name_lower(const char* s) {
    std::string r(s);
    for (char& c : r) c = (char)tolower((unsigned char)c);
    return r;
}

static std::unordered_map<std::string, int> parse_glb_material_indices(
        const std::string& path, std::vector<int>& recolor_out, int& face_out,
        std::vector<int>& additive_out, std::vector<int>& force_opaque_out) {
    std::unordered_map<std::string, int> result;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return result;

    uint32_t magic = 0, version = 0, total_len = 0;
    fread(&magic,     4, 1, f);
    fread(&version,   4, 1, f);
    fread(&total_len, 4, 1, f);

    if (magic != 0x46546C67u) { fclose(f); return result; }

    uint32_t chunk_len = 0, chunk_type = 0;
    fread(&chunk_len,  4, 1, f);
    fread(&chunk_type, 4, 1, f);

    if (chunk_type != 0x4E4F534Au) { fclose(f); return result; }

    std::string json(chunk_len, '\0');
    fread(json.data(), 1, chunk_len, f);
    fclose(f);

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
                force_opaque_out.push_back(raylib_idx);
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
    for (int m = 0; m < model.meshCount; m++) {
        auto& mesh = model.meshes[m];
        if (mesh.colors == nullptr) continue;
        for (int v = 0; v < mesh.vertexCount * 4; v++) mesh.colors[v] = 255;
        ray::UpdateMeshBuffer(mesh, 3, mesh.colors, mesh.vertexCount * 4, 0);
    }

    std::vector<int> recolor_indices, additive_indices, force_opaque_indices;
    int face_material_index = -1;
    auto material_indices = parse_glb_material_indices(model_path.string(), recolor_indices, face_material_index, additive_indices, force_opaque_indices);

    if (normalize_face_scale && face_material_index != -1) {
        constexpr float COS_FACE_PLANE_SIZE = 0.137f;
        for (int m = 0; m < model.meshCount; m++)
            if (model.meshMaterial[m] == face_material_index)
                normalize_face_mesh_size(model.meshes[m], COS_FACE_PLANE_SIZE);
    }
#ifdef PLATFORM_ANDROID
    if (face_material_index != -1 && face_shader.id != 0)
        model.materials[face_material_index].shader = face_shader;
#endif
    additive_indices.erase(
        std::remove(additive_indices.begin(), additive_indices.end(), face_material_index),
        additive_indices.end());
    for (int idx : additive_indices)
        model.materials[idx].maps[ray::MATERIAL_MAP_DIFFUSE].color = {255, 255, 255, 255};
    for (int idx : force_opaque_indices) {
        auto& map = model.materials[idx].maps[ray::MATERIAL_MAP_DIFFUSE];
        if (map.texture.id != 0) {
            ray::Image img = ray::LoadImageFromTexture(map.texture);
            ray::ImageFormat(&img, ray::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            unsigned char* px = (unsigned char*)img.data;
            for (int p = 0; p < img.width * img.height; p++) px[p * 4 + 3] = 255;
            ray::UnloadTexture(map.texture);
            map.texture = ray::LoadTextureFromImage(img);
            ray::UnloadImage(img);
        }
    }

    ray::Model glb_model = ray::LoadModel(anim_path.string().c_str());
    int anim_count = 0;
    ray::ModelAnimation* anims = ray::LoadModelAnimations(anim_path.string().c_str(), &anim_count);
    reindex_animations(model, glb_model, anims, anim_count);
    ray::UnloadModel(glb_model);

    parts.push_back(model);
    part_material_indices.push_back(std::move(material_indices));
    part_recolor_indices.push_back(std::move(recolor_indices));
    part_additive_indices.push_back(std::move(additive_indices));
    part_force_opaque_indices.push_back(std::move(force_opaque_indices));
    part_face_material_index.push_back(face_material_index);
    part_anims.push_back(anims);
    part_anim_count.push_back(anim_count);
}

static void init_shaders(ray::Shader& outline_fxaa_shader, int& outline_fxaa_size_loc, int& outline_fxaa_thickness_loc,
                          ray::Shader& null_shader, ray::Shader& face_shader, ray::Shader& outline_shader,
                          bool& use_render_textures) {
    outline_fxaa_shader = load_shader("shader/pass.vs", "shader/outline_fxaa.fs");
    outline_fxaa_size_loc = ray::GetShaderLocation(outline_fxaa_shader, "texSize");
    outline_fxaa_thickness_loc = ray::GetShaderLocation(outline_fxaa_shader, "outlineThickness");
    float outline_thickness = 3.0f;
    ray::SetShaderValue(outline_fxaa_shader, outline_fxaa_thickness_loc, &outline_thickness, ray::SHADER_UNIFORM_FLOAT);

    null_shader    = load_shader(nullptr, "shader/null.fs");
    face_shader    = load_shader(nullptr, "shader/face.fs");
    outline_shader = load_shader("shader/outline.vs", "shader/outline.fs");
    int thickness_loc = ray::GetShaderLocation(outline_shader, "outlineThickness");
    float thickness = 0.0035f;
    ray::SetShaderValue(outline_shader, thickness_loc, &thickness, ray::SHADER_UNIFORM_FLOAT);

    if (outline_fxaa_shader.id == 0)
        use_render_textures = false;
}

Chara3D::Chara3D(std::string& model_name, bool mirror, bool use_skin_config) {
    init_shaders(outline_fxaa_shader, outline_fxaa_size_loc, outline_fxaa_thickness_loc,
                 null_shader, face_shader, outline_shader, use_render_textures);
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

    model_valid = parts[0].meshCount > 0;

    fs::path face_dir = resolve_skin_path("Models/face");
    load_face_textures(face_dir);

    fs::path skin_anim_path = fs::path("Skins") / global_data.config->paths.skin
                              / "Graphics" / "global" / "animation.json";
    load_face_anims(skin_anim_path);

    set_anim(anim_index);
}

Chara3D::Chara3D(std::string& head_name, std::string& body_name, bool mirror, bool use_skin_config) {
    init_shaders(outline_fxaa_shader, outline_fxaa_size_loc, outline_fxaa_thickness_loc,
                 null_shader, face_shader, outline_shader, use_render_textures);
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

    model_valid = parts[0].meshCount > 0 && parts[1].meshCount > 0;

    fs::path face_dir = resolve_skin_path("Models/face");
    load_face_textures(face_dir);

    fs::path skin_anim_path = fs::path("Skins") / global_data.config->paths.skin
                              / "Graphics" / "global" / "animation.json";
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
    ray::UnloadShader(outline_fxaa_shader);
    if (scene_target.id != 0) ray::UnloadRenderTexture(scene_target);
    ray::UnloadShader(outline_shader);
    for (auto& tex : face_textures)
        ray::UnloadTexture(tex);
}

void Chara3D::set_texture(fs::path& texture_path, int part_index, int material_index) {
    ray::Texture2D old = parts[part_index].materials[material_index].maps[ray::MATERIAL_MAP_DIFFUSE].texture;
    if (old.id != 0) ray::UnloadTexture(old);
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
    ray::Texture2D new_tex = recolor_texture(img, body, face, rim);
    ray::UnloadImage(img);
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
    13, 14, 15, 16, 65, 17, 22, 19, 30, 29, 23, 24, 63, 44,
    40, 41, 42, 43, 44, 45, 46, 58, 59, 62, 60, 18,
    13, 14, 15, 16, 65, 17, 22, 19, 30, 29, 23, 24, 63, 44,
    41, 40, 42, 43, 44, 45, 46, 58, 59, 62, 60, 18,
};

void Chara3D::set_anim(AnimIndex idx) {
    int i = static_cast<int>(idx);
    int anim_count = part_anim_count.empty() ? 0 : part_anim_count[0];
    if (i >= 0 && i < anim_count) {
        if (idx == AnimIndex::DON_NORMAL || idx == AnimIndex::DON_SABI) {
            is_looping = true;
        } else if (get_anim_name(i).find("loop") == std::string::npos) {
            is_looping = false;
            prev_anim_idx = anim_index;
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
        double ms_per_beat = 60000.0 / bpm;
        if (anim_index == AnimIndex::DON_NORMAL || anim_index == AnimIndex::DON_SABI) ms_per_beat *= 3;
        if (anim_index == AnimIndex::DON_BALLOON_LOOP) ms_per_beat /= 2;
        double ms_per_frame = ms_per_beat / part_anims[0][ai].keyframeCount;
        if (current_ms - last_frame_ms >= ms_per_frame) {
            int loop_frames = part_anims[0][ai].keyframeCount - 1;
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
                for (size_t p = 0; p < parts.size(); p++)
                    ray::UpdateModelAnimation(parts[p], part_anims[p][ai], anim_frame);
                render_dirty = true;

                if (!is_looping && anim_frame == loop_frames - 1) {
                    set_anim(prev_anim_idx);
                    is_looping = true;
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

void Chara3D::draw_outline(float x, float y) {
    std::vector<std::vector<ray::Shader>> saved(parts.size());
    for (size_t p = 0; p < parts.size(); p++) {
        saved[p].resize(parts[p].materialCount);
        for (int i = 0; i < parts[p].materialCount; i++) {
            saved[p][i] = parts[p].materials[i].shader;
            bool is_face = (part_face_material_index[p] != -1 && i == part_face_material_index[p] && null_shader.id != 0);
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

    rlSetCullFace(RL_CULL_FACE_FRONT);
    // scale is in 1280x720 virtual units; the camera maps the skin's virtual
    // canvas to the window, so follow the skin resolution or the model
    // shrinks relative to everything else on hi-res skins.
    for (auto& part : parts)
        ray::DrawModel(part, {x, y, 400.0f}, scale * draw_scale * tex.screen_scale, ray::WHITE);
    rlSetCullFace(RL_CULL_FACE_BACK);

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
        draw_model_face_last(parts[p], part_face_material_index[p], {x, y, 400.0f}, scale * draw_scale * tex.screen_scale);
    for (size_t p = 0; p < parts.size(); p++)
        parts[p].transform = saved[p];
}

void Chara3D::draw(float x, float y, float scale_mul) {

    int rw = ray::GetRenderWidth();
    int rh = ray::GetRenderHeight();

    if (scene_target.id == 0 || scene_target_w != rw || scene_target_h != rh) {
        if (scene_target.id != 0) ray::UnloadRenderTexture(scene_target);
        scene_target   = ray::LoadRenderTexture(rw, rh);
        if (scene_target.id == 0) {
            spdlog::warn("Chara3D: render texture unavailable, using direct render");
            use_render_textures = false;
        }
        scene_target_w = rw;
        scene_target_h = rh;
        float ts[2] = {(float)rw, (float)rh};
        ray::SetShaderValue(outline_fxaa_shader, outline_fxaa_size_loc, ts, ray::SHADER_UNIFORM_VEC2);
        render_dirty = true;
    }

    if (!use_render_textures) {
        ray::Camera2D cam2d = compute_camera2d(tex.screen_width, tex.screen_height);
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

    ray::Camera2D cam2d = compute_camera2d(tex.screen_width, tex.screen_height);

    ray::EndMode2D();
    ray::EndBlendMode();

    if (render_dirty) {
        render_dirty = false;
        ray::Camera3D cam3d = camera2d_to_3d(cam2d);

        ray::BeginTextureMode(scene_target);
        ray::ClearBackground(ray::BLANK);
        ray::BeginBlendMode(ray::BLEND_ALPHA);
        ray::BeginMode3D(cam3d);
        draw_outline(x, y);
        draw_3d(x, y);
        ray::EndMode3D();
        ray::EndBlendMode();
        ray::EndTextureMode();
    }

    {
        ray::BeginShaderMode(outline_fxaa_shader);
        ray::DrawTextureRec(scene_target.texture,
            {0, 0, (float)rw, -(float)rh},
            {0, 0}, ray::WHITE);
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
