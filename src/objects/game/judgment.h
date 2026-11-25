#pragma once

#include "../../libs/gfx/texture.h"
#include "../../libs/math/animation.h"
#include "../enums.h"

class Judgment {
private:
    Judgments type;

    FadeAnimation* fade_animation_1;
    FadeAnimation* fade_animation_2;
    FadeAnimation* fade_animation_3 = nullptr; // null when the skin has no animation 77
    MoveAnimation* move_animation;
    TextureChangeAnimation* texture_animation;
    TextureObject* t_effect = nullptr;
    TextureObject* t_outer_effect = nullptr;
    TextureObject* t_text = nullptr;
    bool big_outer = false;
    TextureResizeAnimation* outer_scale = nullptr;
    TextureObject* t_ray = nullptr;
    TextureResizeAnimation* ray_scale = nullptr;
    FadeAnimation* ray_fade = nullptr;
    TextureResizeAnimation* ray2_scale = nullptr;
    FadeAnimation* ray2_fade = nullptr;

public:
    Judgment(Judgments type, bool big);

    void update(double current_ms);

    void draw_effect(float judge_x, float judge_y);

    void draw_ray_effect(float judge_x, float judge_y);

    void draw_outer_effect(float judge_x, float judge_y);

    bool uses_additive() const { return t_outer_effect != nullptr; }

    void draw_text(float judge_x, float judge_y);

    bool is_finished() const;
};
