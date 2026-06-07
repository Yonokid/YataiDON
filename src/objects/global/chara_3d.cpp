#include "chara_3d.h"
#include "../../libs/animation.h"
#include "../../libs/camera_utils.h"
#include "../../libs/global_data.h"
#include <fstream>
#include <rapidjson/document.h>
extern "C" { void rlSetCullFace(int mode); }
static constexpr int RL_CULL_FACE_FRONT = 0;
static constexpr int RL_CULL_FACE_BACK  = 1;

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

Chara3D::Chara3D(std::string& model_name, bool mirror) {
    fxaa_shader   = load_shader("shader/dummy.vs", "shader/fxaa.fs");
    fxaa_size_loc = ray::GetShaderLocation(fxaa_shader, "textureSize");

    outline_pass_shader = load_shader("shader/dummy.vs", "shader/outline_pass.fs");
    outline_pass_size_loc = ray::GetShaderLocation(outline_pass_shader, "textureSize");
    outline_pass_thickness_loc = ray::GetShaderLocation(outline_pass_shader, "outlineThickness");
    float outline_thickness = 3.0f;
    ray::SetShaderValue(outline_pass_shader, outline_pass_thickness_loc, &outline_thickness, ray::SHADER_UNIFORM_FLOAT);

    outline_shader = load_shader("shader/outline.vs", "shader/outline.fs");
    int thickness_loc = ray::GetShaderLocation(outline_shader, "outlineThickness");
    float thickness = 0.0035f;
    ray::SetShaderValue(outline_shader, thickness_loc, &thickness, ray::SHADER_UNIFORM_FLOAT);

    this->mirror = mirror;
    fs::path root_path = fs::path("Skins") / global_data.config->paths.skin / "Models";
    fs::path model_path = root_path / "cos" / (model_name + ".glb");
    fs::path anim_path = root_path / "animations.glb";
    cos_model = ray::LoadModel(model_path.string().c_str());
    for (int m = 0; m < cos_model.meshCount; m++) {
        auto& mesh = cos_model.meshes[m];
        if (mesh.colors == nullptr) continue;
        for (int v = 0; v < mesh.vertexCount * 4; v++) mesh.colors[v] = 255;
        ray::UpdateMeshBuffer(mesh, 3, mesh.colors, mesh.vertexCount * 4, 0);
    }
    material_indices = parse_glb_material_indices(model_path.string(), recolor_indices, face_material_index, additive_indices, force_opaque_indices);
    additive_indices.erase(
        std::remove(additive_indices.begin(), additive_indices.end(), face_material_index),
        additive_indices.end());
    for (int idx : additive_indices)
        cos_model.materials[idx].maps[ray::MATERIAL_MAP_DIFFUSE].color = {255, 255, 255, 255};
    for (int idx : force_opaque_indices) {
        auto& map = cos_model.materials[idx].maps[ray::MATERIAL_MAP_DIFFUSE];
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
    anims = ray::LoadModelAnimations(anim_path.string().c_str(), &anim_count);
    reindex_animations(cos_model, glb_model, anims, anim_count);
    ray::UnloadModel(glb_model);

    fs::path face_dir = root_path / "face";
    load_face_textures(face_dir);

    fs::path skin_anim_path = fs::path("Skins") / global_data.config->paths.skin
                              / "Graphics" / "global" / "animation.json";
    load_face_anims(skin_anim_path);

    set_anim(anim_index);
}

Chara3D::~Chara3D() {
    if (face_material_index != -1 && !face_textures.empty())
        cos_model.materials[face_material_index].maps[ray::MATERIAL_MAP_DIFFUSE].texture.id = 0;
    ray::UnloadModelAnimations(anims, anim_count);
    ray::UnloadModel(cos_model);
    ray::UnloadShader(fxaa_shader);
    ray::UnloadShader(outline_pass_shader);
    if (fxaa_target.id != 0) ray::UnloadRenderTexture(fxaa_target);
    if (scene_target.id != 0) ray::UnloadRenderTexture(scene_target);
    ray::UnloadShader(outline_shader);
    for (auto& tex : face_textures)
        ray::UnloadTexture(tex);
}

void Chara3D::set_texture(fs::path& texture_path, int material_index) {
    ray::Texture2D old = cos_model.materials[material_index].maps[ray::MATERIAL_MAP_DIFFUSE].texture;
    if (old.id != 0) ray::UnloadTexture(old);
    ray::Texture tex = ray::LoadTexture(texture_path.string().c_str());
    ray::GenTextureMipmaps(&tex);
    ray::SetTextureFilter(tex, ray::TEXTURE_FILTER_BILINEAR);
    int map_type = ray::MATERIAL_MAP_DIFFUSE;
    ray::SetMaterialTexture(&cos_model.materials[material_index], map_type, tex);
}

void Chara3D::set_body_texture(fs::path& texture_path) {
    auto it = material_indices.find("RGB_don_color_S_CUS_0x10000001_");
    if (it != material_indices.end()) set_texture(texture_path, it->second);
}

void Chara3D::set_face_rim_texture(fs::path& texture_path) {
    auto it = material_indices.find("don_FACEHIP_color_S_CUS_0x10000001_");
    if (it != material_indices.end()) set_texture(texture_path, it->second);
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
    for (int f = 0; f < 12; f++) {
        ray::Rectangle rect = {0, (float)(f * 128), 128, 128};
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
    if (face_material_index == -1) return;
    if (face_index < 0 || face_index >= (int)face_textures.size()) return;
    cos_model.materials[face_material_index].maps[ray::MATERIAL_MAP_DIFFUSE].texture =
        face_textures[face_index];
    current_face_index = face_index;
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
    for (int idx : recolor_indices)
        apply_don_colors(cos_model, idx, body, face, rim);
}

static constexpr int FACE_ANIM_IDS[] = {
    13, 14, 15, 16, 65, 17, 22, 19, 30, 29, 23, 24, 63, 44,
    40, 41, 42, 43, 44, 45, 46, 58, 59, 62, 60, 18,
    13, 14, 15, 16, 65, 17, 22, 19, 30, 29, 23, 24, 63, 44,
    41, 40, 42, 43, 44, 45, 46, 58, 59, 62, 60, 18,
};

void Chara3D::set_anim(AnimIndex idx) {
    int i = static_cast<int>(idx);
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
    if (idx >= 0 && idx < anim_count) {
        return anims[idx].name;
    }
    return "";
}

void Chara3D::set_bpm(float bpm) {
    this->bpm = bpm;
}

int Chara3D::get_anim_count() const {
    return anim_count;
}

void Chara3D::update(double current_ms) {
    if (anim_count > 0) {
        double ms_per_beat = 60000.0 / bpm;
        if (anim_index == AnimIndex::DON_NORMAL || anim_index == AnimIndex::DON_SABI) ms_per_beat *= 3;
        if (anim_index == AnimIndex::DON_BALLOON_LOOP) ms_per_beat /= 2;
        int ai = static_cast<int>(anim_index);
        double ms_per_frame = ms_per_beat / anims[ai].keyframeCount;
        if (current_ms - last_frame_ms >= ms_per_frame) {
            int loop_frames = anims[ai].keyframeCount - 1;
            anim_frame = (anim_frame + 1) % loop_frames;
            ray::UpdateModelAnimation(cos_model, anims[ai], anim_frame);
            // Push updated vertex/normal/uv buffers to GPU after animation update.
            for (int m = 0; m < cos_model.meshCount; m++) {
                auto& mesh = cos_model.meshes[m];
                if (mesh.vertexCount <= 0) continue;
                // Vertex positions (location 0)
                if (mesh.animVertices != nullptr)
                    ray::UpdateMeshBuffer(mesh, 0, mesh.animVertices, mesh.vertexCount * 3 * sizeof(float), 0);
                else if (mesh.vertices != nullptr)
                    ray::UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * sizeof(float), 0);

                // Normals (location 2)
                if (mesh.animNormals != nullptr)
                    ray::UpdateMeshBuffer(mesh, 2, mesh.animNormals, mesh.vertexCount * 3 * sizeof(float), 0);
                else if (mesh.normals != nullptr)
                    ray::UpdateMeshBuffer(mesh, 2, mesh.normals, mesh.vertexCount * 3 * sizeof(float), 0);

                // UVs (primary texcoords: location 1)
                if (mesh.texcoords != nullptr)
                    ray::UpdateMeshBuffer(mesh, 1, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
                // secondary texcoords (location 5)
                if (mesh.texcoords2 != nullptr)
                    ray::UpdateMeshBuffer(mesh, 5, mesh.texcoords2, mesh.vertexCount * 2 * sizeof(float), 0);
            }
            last_frame_ms = current_ms;

            if (!is_looping && anim_frame == loop_frames - 1) {
                set_anim(prev_anim_idx);
                is_looping = true;
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
    std::vector<ray::Shader> saved(cos_model.materialCount);
    for (int i = 0; i < cos_model.materialCount; i++) {
        saved[i] = cos_model.materials[i].shader;
        cos_model.materials[i].shader = outline_shader;
    }

    ray::Matrix saved_transform = cos_model.transform;
    float y_angle = mirror ? -rot_y : rot_y;
    cos_model.transform = rotation_xyz(rot_x * DEG2RAD, y_angle * DEG2RAD, rot_z * DEG2RAD);

    rlSetCullFace(RL_CULL_FACE_FRONT);
    ray::DrawModel(cos_model, {x, y, 400.0f}, scale, ray::WHITE);
    rlSetCullFace(RL_CULL_FACE_BACK);

    cos_model.transform = saved_transform;
    for (int i = 0; i < cos_model.materialCount; i++)
        cos_model.materials[i].shader = saved[i];
}

void Chara3D::draw_3d(float x, float y) {
    ray::Matrix saved = cos_model.transform;
    float y_angle = mirror ? -rot_y : rot_y;
    cos_model.transform = rotation_xyz(rot_x * DEG2RAD, y_angle * DEG2RAD, rot_z * DEG2RAD);

    ray::DrawModel(cos_model, {x, y, 400.0f}, scale, ray::WHITE);

    cos_model.transform = saved;
}

void Chara3D::draw(float x, float y) {
    int rw = ray::GetRenderWidth();
    int rh = ray::GetRenderHeight();

    if (scene_target.id == 0 || scene_target_w != rw || scene_target_h != rh) {
        if (scene_target.id != 0) ray::UnloadRenderTexture(scene_target);
        scene_target   = ray::LoadRenderTexture(rw, rh);
        scene_target_w = rw;
        scene_target_h = rh;
        float ts[2] = {(float)rw, (float)rh};
        ray::SetShaderValue(outline_pass_shader, outline_pass_size_loc, ts, ray::SHADER_UNIFORM_VEC2);
    }

    if (fxaa_target.id == 0 || fxaa_target_w != rw || fxaa_target_h != rh) {
        if (fxaa_target.id != 0) ray::UnloadRenderTexture(fxaa_target);
        fxaa_target   = ray::LoadRenderTexture(rw, rh);
        fxaa_target_w = rw;
        fxaa_target_h = rh;
        float ts[2] = {(float)rw, (float)rh};
        ray::SetShaderValue(fxaa_shader, fxaa_size_loc, ts, ray::SHADER_UNIFORM_VEC2);
    }

    ray::Camera2D cam2d = compute_camera2d(tex.screen_width, tex.screen_height);
    ray::Camera3D cam3d = camera2d_to_3d(cam2d);

    ray::EndMode2D();
    ray::EndBlendMode();

    ray::BeginTextureMode(scene_target);
    ray::ClearBackground(ray::BLANK);
    ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE);
    ray::BeginMode3D(cam3d);
    draw_outline(x, y);
    draw_3d(x, y);
    ray::EndMode3D();
    ray::EndBlendMode();
    ray::EndTextureMode();

    ray::BeginTextureMode(fxaa_target);
    ray::ClearBackground(ray::BLANK);
    ray::BeginShaderMode(outline_pass_shader);
    ray::DrawTextureRec(scene_target.texture,
        {0, 0, (float)rw, -(float)rh},
        {0, 0}, ray::WHITE);
    ray::EndShaderMode();
    ray::EndTextureMode();

    ray::BeginShaderMode(fxaa_shader);
    ray::DrawTextureRec(fxaa_target.texture,
        {0, 0, (float)rw, -(float)rh},
        {0, 0}, ray::WHITE);
    ray::EndShaderMode();

    ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE);
    ray::BeginMode2D(cam2d);
}
