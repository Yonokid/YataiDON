#pragma once

#include "../../libs/texture.h"
#include "../../libs/ray.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class BaseAnimation;

enum class AnimIndex : int {
    DON_BALLOON_FAILURE        = 0,
    DON_BALLOON_LOOP           = 1,
    DON_BALLOON_NOBEAT         = 2,
    DON_BALLOON_SUCCESS        = 3,
    DON_BIND                   = 4,
    DON_COMBO                  = 5,
    DON_ENTRY_IN               = 6,
    DON_ENTRY_LOOP             = 7,
    DON_FUKKATU_LOOP           = 8,
    DON_FUKKATU_START          = 9,
    DON_FULL_COMBO             = 10,
    DON_FULL_GAGE              = 11,
    DON_GENERAL_JUMP           = 12,
    DON_GENERAL_LOOP           = 13,
    DON_MISS                   = 14,
    DON_MISS6                  = 15,
    DON_MISS_NORMAL            = 16,
    DON_NORM_DOWN              = 17,
    DON_NORM_LOOP              = 18,
    DON_NORM_UP                = 19,
    DON_NORMAL                 = 20,
    DON_SABI                   = 21,
    DON_SABI_START             = 22,
    DON_SELECT_LOOP            = 23,
    DON_SELECT_PANELDOWN       = 24,
    DON_SELECT_PANELUP         = 25,
    DON_BALLOON_FAILURE_MIRROR = 26,
    DON_BALLOON_LOOP_MIRROR    = 27,
    DON_BALLOON_NOBEAT_MIRROR  = 28,
    DON_BALLOON_SUCCESS_MIRROR = 29,
    DON_BIND_MIRROR            = 30,
    DON_COMBO_MIRROR           = 31,
    DON_ENTRY_IN_MIRROR        = 32,
    DON_ENTRY_LOOP_MIRROR      = 33,
    DON_FUKKATU_LOOP_MIRROR    = 34,
    DON_FUKKATU_START_MIRROR   = 35,
    DON_FULL_COMBO_MIRROR      = 36,
    DON_FULL_GAGE_MIRROR       = 37,
    DON_GENERAL_JUMP_MIRROR    = 38,
    DON_GENERAL_LOOP_MIRROR    = 39,
    DON_MISS6_MIRROR           = 40,
    DON_MISS_MIRROR            = 41,
    DON_MISS_NORMAL_MIRROR     = 42,
    DON_NORM_DOWN_MIRROR       = 43,
    DON_NORM_LOOP_MIRROR       = 44,
    DON_NORM_UP_MIRROR         = 45,
    DON_NORMAL_MIRROR          = 46,
    DON_SABI_MIRROR            = 47,
    DON_SABI_START_MIRROR      = 48,
    DON_SELECT_LOOP_MIRROR     = 49,
    DON_SELECT_PANELDOWN_MIRROR = 50,
    DON_SELECT_PANELUP_MIRROR  = 51,
};

class Chara3D {
private:
    ray::Model cos_model;
    std::unordered_map<std::string, int> material_indices;
    std::vector<int> recolor_indices;
    std::vector<int> additive_indices;
    std::vector<int> force_opaque_indices;
    int face_material_index = -1;

    std::vector<ray::Texture2D> face_textures;
    std::unordered_map<int, std::unique_ptr<BaseAnimation>> face_anims;
    BaseAnimation* current_face_anim = nullptr;
    int current_face_index = -1;

    ray::ModelAnimation* anims;
    int anim_count = 0;
    int anim_frame = 0;
    AnimIndex anim_index = AnimIndex::DON_NORMAL;

    float bpm = 100;
    bool mirror = false;
    double last_frame_ms = 0;

    float scale = 650.0f;
    float rot_x = 180.0f;
    float rot_y = 20.0f;
    float rot_z = 0.0f;

    AnimIndex prev_anim_idx = AnimIndex::DON_BALLOON_FAILURE;
    bool is_looping = true;

    ray::Shader fxaa_shader;
    int fxaa_size_loc = -1;
    ray::RenderTexture2D fxaa_target = {};
    int fxaa_target_w = 0;
    int fxaa_target_h = 0;

    ray::Shader outline_shader;

    void set_texture(fs::path& texture_path, int material_index);
    void load_face_textures(fs::path& face_dir);
    void load_face_anims(fs::path& anim_path);
    void apply_face(int face_index);
    void draw_outline(float x, float y);
    void draw_3d(float x, float y);
public:
    Chara3D(std::string& model_name, bool mirror = false);

    ~Chara3D();

    void set_body_texture(fs::path& texture_path);
    void set_face_rim_texture(fs::path& texture_path);

    void set_don_colors(ray::Color body, ray::Color face, ray::Color rim);

    void set_bpm(float bpm);
    void set_anim(AnimIndex idx);
    int  get_anim_count() const;
    std::string get_anim_name(int idx);

    void update(double current_ms);

    void draw(float x, float y);
};
