#pragma once

#include <cmath>
#include "../../libs/text.h"
#include "../../enums.h"
#include "box_yellow.h"

struct BoxDef {
    std::string name;
    TextureIndex texture_index;
    GenreIndex genre_index;
    std::string collection;
    std::optional<ray::Color> back_color;
    std::optional<ray::Color> fore_color;
    std::optional<ray::Color> box_color;
};

inline size_t utf8_char_count(const std::string& s) {
    size_t count = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) ++count;
    }
    return count;
}

class BaseBox {
public:
    bool text_loaded = false;
    GenreIndex genre_index;
    std::string text_name;
    std::string collection;

    TextureIndex texture_index;
    std::optional<ray::Color> back_color;
    std::optional<ray::Color> fore_color;
    ray::Color text_color = ray::WHITE;

    FadeAnimation* fade;
    MoveAnimation* open_anim;
    FadeAnimation* open_fade;

    float position;
    float cross_pos = 0.0f;
    float cross_target = 0.0f;
    float cross_lead   = 0.0f;   // cross_pos - cross_target at glide start
    float move_delta   = 0.0f;   // row-axis span of the active move (0 = none)
    void snap_cross(float x)  { cross_pos = cross_target = x; cross_lead = 0.0f; }
    void glide_cross(float x) { cross_lead = cross_pos - x; cross_target = x; }
    bool vertical = false;
    float left_bound;
    float right_bound;

    float box_x() const { return vertical ? cross_pos : position; }
    float box_y() const { return vertical ? position  : 0.0f; }

    fs::path path;

    bool is_new = false;
    bool preserve_order = false;

    BaseBox(const fs::path& path, const BoxDef& box_def);
    virtual ~BaseBox();

    virtual void load_text();
    virtual void get_scores() {}
    virtual void draw_score_history() {}
    virtual void draw_diff_select();
    virtual void draw_diff_select_bg() {}

    virtual void reset();
    void set_position(float target_position);
    virtual void expand_box();
    virtual void close_box();

    virtual void enter_box();
    virtual void exit_box();

    void fade_in(float delay);
    void fade_out();

    void move_box(float target_position, float duration);
    virtual void update(double current_ms);

    virtual void draw();

    const char* draw_state() const {
        if (yellow_box.has_value() && yellow_box->is_diff_select) return "diff_select";
        if (yellow_box.has_value() && yellow_box_opened) return "open";
        return "closed";
    }
    virtual const char* lua_kind() const { return "box"; }
    float bar_anime_count() const {
        double elapsed = get_current_ms() - bar_open_started_at;
        if (elapsed <= 200.0) return 0.0f;
        float deg = std::min((float)(elapsed - 200.0) * 1.5f, 90.0f);
        return std::sin(deg * 3.14159265f / 180.0f) * 62.0f;
    }
    OutlinedText* name_text() const { return name.get(); }

    OutlinedText* horizontal_name() {
        if (!horizontal_name_cache) {
            float font_size = tex.skin_config[SC::SONG_BOX_NAME].font_size;
            if (utf8_char_count(text_name) >= 30)
                font_size -= (int)(10 * tex.screen_scale);
            horizontal_name_cache = std::make_unique<OutlinedText>(text_name, font_size, text_color, fore_color.value(), false);
        }
        return horizontal_name_cache.get();
    }

    OutlinedText* horizontal_name_large() {
        if (!horizontal_name_large_cache) {
            float font_size = tex.skin_config[SC::SONG_BOX_NAME].font_size;
            if (utf8_char_count(text_name) >= 30)
                font_size -= (int)(10 * tex.screen_scale);
            horizontal_name_large_cache = std::make_unique<OutlinedText>(text_name, (int)(font_size * 1.5f), text_color, fore_color.value(), false, 6);
        }
        return horizontal_name_large_cache.get();
    }

protected:
    std::unique_ptr<MoveAnimation> move;

    ray::Shader shader;
    bool shader_loaded = false;

    std::unique_ptr<OutlinedText> name;
    std::unique_ptr<OutlinedText> horizontal_name_cache;
    std::unique_ptr<OutlinedText> horizontal_name_large_cache;

    std::optional<YellowBox> yellow_box;
    bool yellow_box_opened = false;

    float target_position;
    double bar_open_started_at = 0.0;

    virtual void draw_closed();
    virtual void draw_open();
};
