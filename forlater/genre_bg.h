#pragma once
#include "box_base.h"
#include "../../libs/text.h"
#include "../../libs/animation.h"
#include <optional>

class GenreBG {
public:
    OutlinedText* title;
    BaseBox* start_box;
    BaseBox* end_box;
    float start_position;
    float end_position_final;
    float end_position;
    std::optional<int> diff_num;
    std::optional<ray::Color> color;
    ray::Shader shader;
    bool shader_loaded;

    FadeAnimation* fade_in;
    MoveAnimation* move;
    FadeAnimation* box_fade_in;

    GenreBG(BaseBox* start_box, BaseBox* end_box, OutlinedText* title, std::optional<int> diff_sort);
    ~GenreBG();

    void load_shader();
    void update(double current_ms);
    void draw(float y);
};
