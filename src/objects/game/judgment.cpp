#include "judgment.h"

Judgment::Judgment(Judgments type, bool big, bool is_2p)
    : is_2p(is_2p), type(type), big(big) {

    fade_animation_1 = (FadeAnimation*)tex.get_animation(27, true);
    fade_animation_2 = (FadeAnimation*)tex.get_animation(28, true);
    move_animation = (MoveAnimation*)tex.get_animation(29, true);
    texture_animation = (TextureChangeAnimation*)tex.get_animation(30, true);

    move_animation->start();
    fade_animation_2->start();
    fade_animation_1->start();
    texture_animation->start();
}

void Judgment::update(double current_ms) {
    BaseAnimation* animations[] = {
        fade_animation_1,
        fade_animation_2,
        move_animation,
        texture_animation
    };

    for (int i = 0; i < 4; i++) {
        animations[i]->update(current_ms);
    }
}

void Judgment::draw(float judge_x, float judge_y) {
    float y = move_animation->attribute;
    int index = static_cast<int>(texture_animation->attribute);
    float hit_fade = fade_animation_1->attribute;
    float fade = fade_animation_2->attribute;

    if (type == Judgments::GOOD) {
        if (big) {
            tex.draw_texture("hit_effect", "hit_effect_good_big", {.x=judge_x, .y=judge_y, .fade=fade, .index=is_2p});
            tex.draw_texture("hit_effect", "outer_good_big",{.x=judge_x, .y=judge_y, .fade=hit_fade, .index=is_2p});
        } else {
            tex.draw_texture("hit_effect", "hit_effect_good", {.x=judge_x, .y=judge_y, .fade=fade, .index=is_2p});
            tex.draw_texture("hit_effect", "outer_good", {.x=judge_x, .y=judge_y, .fade=hit_fade, .index=is_2p});
        }
        tex.draw_texture("hit_effect", "judge_good", {.x=judge_x, .y=y + judge_y, .fade=fade, .index=is_2p});
    }
    else if (type == Judgments::OK) {
        if (big) {
            tex.draw_texture("hit_effect", "hit_effect_ok_big", {.x=judge_x, .y=judge_y, .fade=fade, .index=is_2p});
            tex.draw_texture("hit_effect", "outer_ok_big", {.x=judge_x, .y=judge_y, .fade=hit_fade, .index=is_2p});
        } else {
            tex.draw_texture("hit_effect", "hit_effect_ok", {.x=judge_x, .y=judge_y, .fade=fade, .index=is_2p});
            tex.draw_texture("hit_effect", "outer_ok", {.x=judge_x, .y=judge_y, .fade=hit_fade, .index=is_2p});
        }
        tex.draw_texture("hit_effect", "judge_ok", {.x=judge_x, .y=y + judge_y, .fade=fade, .index=is_2p});
    }
    else if (type == Judgments::BAD) {
        tex.draw_texture("hit_effect", "judge_bad", {.x=judge_x, .y=y + judge_y, .fade=fade, .index=is_2p});
    }
}

bool Judgment::is_finished() const {
    return fade_animation_2->is_finished;
}
