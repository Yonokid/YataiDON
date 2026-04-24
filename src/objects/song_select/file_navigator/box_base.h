#pragma once
#include "../../libs/texture.h" // IWYU pragma: keep
#include "../../libs/audio.h" // IWYU pragma: keep
#include "../../libs/text.h"
#include "../../libs/animation.h"
#include "../../enums.h"
#include "color_utils.h" // IWYU pragma: keep
#include "box_yellow.h"

struct BoxDef {
    std::string name;
    TextureIndex texture_index;
    GenreIndex genre_index;
    std::string collection;
    std::optional<ray::Color> back_color;
    std::optional<ray::Color> fore_color;
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

    FadeAnimation* fade;

    float position;
    float left_bound;
    float right_bound;

    fs::path path;

    bool is_new = false;
    bool preserve_order = false;

    BaseBox(const fs::path& path, const BoxDef& box_def);
    virtual ~BaseBox();

    virtual void load_text();
    virtual void get_scores() {}
    virtual void draw_score_history() {}
    virtual void draw_diff_select(bool is_ura);

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

    virtual void draw(bool is_ura);

protected:
    MoveAnimation* open_anim;
    FadeAnimation* open_fade;
    std::unique_ptr<MoveAnimation> move;

    ray::Shader shader;
    bool shader_loaded = false;

    std::unique_ptr<OutlinedText> name;

    std::optional<YellowBox> yellow_box;
    bool yellow_box_opened = false;

    float target_position;

    virtual void draw_closed();
    virtual void draw_open();
};
