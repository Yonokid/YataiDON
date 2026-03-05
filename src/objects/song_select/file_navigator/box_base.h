#pragma once
#include "../../libs/texture.h" // IWYU pragma: keep
#include "../../libs/audio.h" // IWYU pragma: keep
#include "../../libs/text.h"
#include "../../libs/animation.h"
#include "../../enums.h"
#include "color_utils.h" // IWYU pragma: keep
#include "box_yellow.h"

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

    float position;

    fs::path path;

    BaseBox(const fs::path& path, const std::optional<ray::Color> back_color,
            const std::optional<ray::Color> fore_color, TextureIndex texture_index);
    virtual ~BaseBox();

    virtual void load_text();
    virtual void get_scores() {}
    virtual void draw_score_history() {}
    virtual void draw_diff_select(std::optional<float> fade_override);

    void reset_yellow_box();
    void set_position(float target_position);
    virtual void expand_box();
    void close_box();

    void enter_diff_select();
    void exit_diff_select();

    void move_box(float target_position, float duration);
    virtual void update(double current_ms);

    virtual void draw(std::optional<float> inner_fade_override = std::nullopt, float outer_fade_override = 1.0f);

protected:
    MoveAnimation* open_anim;
    FadeAnimation* open_fade;
    std::unique_ptr<MoveAnimation> move;

    ray::Shader shader;
    bool shader_loaded = false;

    std::unique_ptr<OutlinedText> name;

    std::optional<YellowBox> yellow_box;
    bool yellow_box_opened = false;

    TextureIndex texture_index;
    std::optional<ray::Color> back_color;
    std::optional<ray::Color> fore_color;

    float target_position;

    virtual void draw_closed(float outer_fade_override);
    virtual void draw_open(std::optional<float> fade_override);
};
