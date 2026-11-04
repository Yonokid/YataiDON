#pragma once

#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../libs/gfx.h"
#include "../../libs/gfx/color_utils.h"
#include "../../libs/gfx/texture.h"

class BaseAnimation;

enum class AnimIndex : int {
    DON_BALLOON_FAILURE = 0,
    DON_BALLOON_LOOP = 1,
    DON_BALLOON_NOBEAT = 2,
    DON_BALLOON_SUCCESS = 3,
    DON_BIND = 4,
    DON_COMBO = 5,
    DON_ENTRY_IN = 6,
    DON_ENTRY_LOOP = 7,
    DON_FUKKATU_LOOP = 8,
    DON_FUKKATU_START = 9,
    DON_FULL_COMBO = 10,
    DON_FULL_GAGE = 11,
    DON_GENERAL_JUMP = 12,
    DON_GENERAL_LOOP = 13,
    DON_MISS = 14,
    DON_MISS6 = 15,
    DON_MISS_NORMAL = 16,
    DON_NORM_DOWN = 17,
    DON_NORM_LOOP = 18,
    DON_NORM_UP = 19,
    DON_NORMAL = 20,
    DON_SABI = 21,
    DON_SABI_START = 22,
    DON_SELECT_LOOP = 23,
    DON_SELECT_PANELDOWN = 24,
    DON_SELECT_PANELUP = 25,
    DON_BALLOON_FAILURE_MIRROR = 26,
    DON_BALLOON_LOOP_MIRROR = 27,
    DON_BALLOON_NOBEAT_MIRROR = 28,
    DON_BALLOON_SUCCESS_MIRROR = 29,
    DON_BIND_MIRROR = 30,
    DON_COMBO_MIRROR = 31,
    DON_ENTRY_IN_MIRROR = 32,
    DON_ENTRY_LOOP_MIRROR = 33,
    DON_FUKKATU_LOOP_MIRROR = 34,
    DON_FUKKATU_START_MIRROR = 35,
    DON_FULL_COMBO_MIRROR = 36,
    DON_FULL_GAGE_MIRROR = 37,
    DON_GENERAL_JUMP_MIRROR = 38,
    DON_GENERAL_LOOP_MIRROR = 39,
    DON_MISS6_MIRROR = 40,
    DON_MISS_MIRROR = 41,
    DON_MISS_NORMAL_MIRROR = 42,
    DON_NORM_DOWN_MIRROR = 43,
    DON_NORM_LOOP_MIRROR = 44,
    DON_NORM_UP_MIRROR = 45,
    DON_NORMAL_MIRROR = 46,
    DON_SABI_MIRROR = 47,
    DON_SABI_START_MIRROR = 48,
    DON_SELECT_LOOP_MIRROR = 49,
    DON_SELECT_PANELDOWN_MIRROR = 50,
    DON_SELECT_PANELUP_MIRROR = 51,
};

class Chara3D {
private:
    std::vector<Model> parts;
    std::vector<std::unordered_map<std::string, int>> part_material_indices;
    std::vector<std::vector<int>> part_recolor_indices;
    std::vector<std::vector<int>> part_additive_indices;
    std::vector<std::vector<int>> part_cutout_indices; // alpha-tested (_AT_ZERO_ / _AT_ONE_) materials
    std::vector<std::vector<int>>
        part_twosided_indices; // _CULLNONE materials: drawn without back-face culling, no hull
    std::vector<std::vector<int>>
        part_blend_indices; // alpha-blended (_A_AB / glTF BLEND) materials: drawn after the face
    std::vector<int> part_face_material_index;
    std::vector<ModelAnimation*> part_anims;
    std::vector<int> part_anim_count;

    void load_part(const fs::path& model_path, const fs::path& anim_path, bool normalize_face_scale = false);

    std::vector<Texture> face_textures;
    std::unordered_map<int, std::unique_ptr<BaseAnimation>> face_anims;
    BaseAnimation* current_face_anim = nullptr;
    int current_face_index = -1;

    int anim_frame = 0;
    int applied_anim = -1;
    int applied_frame = -1;
    AnimIndex anim_index = AnimIndex::DON_NORMAL;

    float bpm = 100;
    bool mirror = false;
    double last_frame_ms = 0;

    float scale = 1.0f;
    float draw_scale = 1.0f;
    float rot_x = 0.0f;
    float rot_y = 0.0f;
    float rot_z = 0.0f;

    AnimIndex prev_anim_idx = AnimIndex::DON_NORMAL;
    bool is_looping = true;
    bool frozen = false;
    void apply_pose();
    bool use_render_textures = true;

    RenderTarget scene_target;
    int scene_target_w = 0;
    int scene_target_h = 0;

    // Cached-render invalidation: RT passes rerun only when pose/face/texture
    // or draw position changes
    bool render_dirty = true;
    float last_draw_x = std::numeric_limits<float>::lowest();
    float last_draw_y = std::numeric_limits<float>::lowest();

    // Region rendering: scene_target covers only the character's screen rectangle (at 2x),
    // placed at region_x/y (render pixels). full_target = scene_target is the whole screen.
    bool full_target = false;
    int region_x = 0;
    int region_y = 0;
    float last_zoom = 0.0f;
    float last_cam_x = 0.0f;
    float last_cam_y = 0.0f;
    int last_rw = 0;
    int last_rh = 0;

    Shader null_shader;
    Shader face_shader;
    Shader cutout_shader;
    Shader outline_shader;
    int outline_param_loc = -1;
    int outline_size_loc = -1;

    Shader outline_fxaa_shader;
    int outline_fxaa_size_loc = -1;
    int outline_fxaa_thickness_loc = -1;
    int outline_fxaa_tint_color_loc = -1;
    int outline_fxaa_tint_amount_loc = -1;

    void set_texture(fs::path& texture_path, int part_index, int material_index);
    void load_face_textures(fs::path& face_dir);
    void load_face_anims(fs::path& anim_path);
    void draw_outline(float x, float y, int rt_w, int rt_h, float full_h);
    void draw_3d(float x, float y);
    void draw_full_target(float x, float y, int rw, int rh, const Camera2D& cam2d);
    bool screen_bounds(float x, float y, const Camera3D& cam3d, float zoom, int rw, int rh, float& x0, float& y0,
                       float& x1, float& y1);
    void ensure_render_targets(int rw, int rh);
    void draw_body(float x, float y);
    void draw_puchi(float x, float y);

    Texture puchi_tex;
    Texture puchi_silhouette;
    Color tint_color;
    float tint_amount = 0.0f;
    int puchi_index = 0;
    double puchi_ms = 0;
    void prewarm_render_targets();

public:
    Chara3D(std::string& model_name, bool mirror = false, bool use_skin_config = false);
    Chara3D(std::string& head_name, std::string& body_name, bool mirror = false, bool use_skin_config = false);

    ~Chara3D();

    void apply_face(int face_index);
    void set_body_texture(fs::path& texture_path);
    void set_face_rim_texture(fs::path& texture_path);
    void set_puchi(int index);

    void set_don_colors(Color body, Color face, Color rim);

    void set_tint(Color color, float amount);
    void set_bpm(float bpm);
    void set_anim(AnimIndex idx);
    void set_rotation(float x, float y, float z);
    void use_default_pose(bool mirror);
    void set_frozen(bool value);
    void set_anim_progress(float progress);
    int get_anim_count() const;
    std::string get_anim_name(int idx);

    void update(double current_ms);

    void draw(float x, float y, float scale_mul = 1.0f);
};

struct PlayerData;
std::unique_ptr<Chara3D> make_chara_from_player_data(const PlayerData* pd, bool mirror = false,
                                                     bool use_skin_config = false);
