#pragma once

#include <cmath>
#include <numbers>

#include "../../../libs/gfx/color_utils.h"
#include "../../../libs/gfx/text.h"
#include "../../enums.h"

struct BoxDef {
    std::string name;
    TextureIndex texture_index;
    GenreIndex genre_index;
    std::string genre_label;
    std::string collection;
    std::optional<Color> back_color;
    std::optional<Color> fore_color;
    std::optional<Color> box_color;
    std::array<std::string, 3> explanation;
};

inline size_t utf8_char_count(const std::string& s) {
    size_t count = 0;
    for (unsigned char c : strip_text_markup(s)) {
        if ((c & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
}

class BaseBox {
public:
    bool text_loaded = false;
    GenreIndex genre_index;
    std::string genre_label;
    std::string text_name;
    std::string collection;
    std::array<std::string, 3> explanation;

    TextureIndex texture_index;
    std::optional<Color> back_color;
    Color fore_color;
    Color text_color = Color::WHITE;

    std::unique_ptr<FadeAnimation> fade;
    std::unique_ptr<MoveAnimation> open_anim;
    std::unique_ptr<FadeAnimation> open_fade;

    float position;
    float cross_pos = 0.0f;
    float cross_target = 0.0f;
    float cross_lead = 0.0f; // cross_pos - cross_target at glide start
    float move_delta = 0.0f; // row-axis span of the active move (0 = none)
    void snap_cross(float x) {
        cross_pos = cross_target = x;
        cross_lead = 0.0f;
    }
    void glide_cross(float x) {
        cross_lead = cross_pos - x;
        cross_target = x;
    }
    bool vertical = false;
    float left_bound = 0.0f;
    float right_bound = 0.0f;

    float box_x() const { return vertical ? cross_pos : position; }
    float box_y() const { return vertical ? position : 0.0f; }

    fs::path path;

    bool is_new = false;
    bool preserve_order = false;

    BaseBox(fs::path path, const BoxDef& box_def);
    virtual ~BaseBox();

    virtual void load_text();

    virtual void preregister_text();
    virtual void get_scores() {}
    virtual void draw_score_history() {}

    virtual void reset();

    void set_position(float target_position);
    virtual void expand_box();
    virtual void close_box();

    virtual void enter_box();
    virtual void exit_box();

    void fade_in(float delay);
    void fade_out();

    void move_box(float target_position, float duration, float delay = 0.0f,
                  std::optional<EaseType> ease = EaseType::Cubic);
    virtual void update(double current_time);

    const char* draw_state() const {
        if (yellow_box_active && is_diff_select) {
            return "diff_select";
        }
        if (yellow_box_active && yellow_box_opened) {
            return "open";
        }
        return "closed";
    }
    virtual const char* lua_kind() const { return "box"; }
    float bar_anime_count() const {
        double elapsed = get_current_ms() - bar_open_started_at;
        if (elapsed <= 200.0) {
            return 0.0f;
        }
        float deg = std::min(static_cast<float>(elapsed - 200.0) * 1.5f, 90.0f);
        return std::sin(deg * std::numbers::pi_v<float> / 180.0f) * 62.0f;
    }
    OutlinedText* name_text() const { return name.get(); }

    // The base/folder boards recolour their green folder texture through a shader when a
    // genre carries a custom box/back colour. Lua skins cannot express a shader, so the
    // recolor is exposed as begin/end guards wrapping the same draw_texture calls the skin
    // already makes (see draw_closed() / draw_open_bg()).
    bool has_recolor() const { return shader_loaded && texture_index == TextureIndex::NONE; }
    void begin_recolor() {
        if (shader_loaded) {
            begin_shader_mode(shader);
        }
    }
    void end_recolor() {
        if (shader_loaded) {
            end_shader_mode();
        }
    }

    OutlinedText* horizontal_name() {
        if (!horizontal_name_cache) {
            float font_size = tex.get_skin_config(SC::SONG_SELECT_BOX_NAME)->font_size;
            if (utf8_char_count(text_name) >= 30) {
                font_size -= static_cast<int>(10 * tex.screen_scale);
            }
            horizontal_name_cache = std::make_unique<OutlinedText>(text_name, font_size, text_color, fore_color, false);
        }
        return horizontal_name_cache.get();
    }

    OutlinedText* horizontal_name_large() {
        if (!horizontal_name_large_cache) {
            float font_size = tex.get_skin_config(SC::SONG_SELECT_BOX_NAME)->font_size;
            if (utf8_char_count(text_name) >= 30) {
                font_size -= static_cast<int>(10 * tex.screen_scale);
            }
            horizontal_name_large_cache = std::make_unique<OutlinedText>(text_name, static_cast<int>(font_size * 1.5f),
                                                                         text_color, fore_color, false, 6);
        }
        return horizontal_name_large_cache.get();
    }

protected:
    std::unique_ptr<MoveAnimation> move;

    Shader shader;
    std::shared_ptr<Shader> shader_owner;
    bool shader_loaded = false;

    std::unique_ptr<OutlinedText> name;
    std::unique_ptr<OutlinedText> horizontal_name_cache;
    std::unique_ptr<OutlinedText> horizontal_name_large_cache;

    // Box state: whether the box is expanded/opened and in diff-select. The yellow-box
    // geometry and animation driving now live in Lua; the engine only tracks the state and
    // derives the box bounds from the (Lua-advanced) animation attributes.
    bool yellow_box_active = false;
    bool yellow_box_opened = false;
    bool is_diff_select = false;
    float yellow_right_width = 0.0f;        // yellow_box_right texture width (non-diff right bound)
    float folder_texture_left_width = 0.0f; // closed-box left/right bounds
    float folder_texture_right_width = 0.0f;

    float target_position;
    double bar_open_started_at = 0.0;
};
