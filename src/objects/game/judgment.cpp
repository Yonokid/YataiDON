#include "judgment.h"
#include "../../libs/texture.h"

Judgment::Judgment(Judgments type, bool big)
    : type(type), big(big) {

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
            tex.draw_texture(HIT_EFFECT::HIT_EFFECT_GOOD_BIG, {.x=judge_x, .y=judge_y, .fade=fade});
            tex.draw_texture(HIT_EFFECT::OUTER_GOOD_BIG,{.frame=index, .x=judge_x, .y=judge_y, .fade=hit_fade});
        } else {
            tex.draw_texture(HIT_EFFECT::HIT_EFFECT_GOOD, {.x=judge_x, .y=judge_y, .fade=fade});
            tex.draw_texture(HIT_EFFECT::OUTER_GOOD, {.frame=index, .x=judge_x, .y=judge_y, .fade=hit_fade});
        }
        tex.draw_texture(HIT_EFFECT::JUDGE_GOOD, {.frame=index, .x=judge_x, .y=y + judge_y, .fade=fade});
    }
    else if (type == Judgments::OK) {
        if (big) {
            tex.draw_texture(HIT_EFFECT::HIT_EFFECT_OK_BIG, {.x=judge_x, .y=judge_y, .fade=fade});
            tex.draw_texture(HIT_EFFECT::OUTER_OK_BIG, {.frame=index, .x=judge_x, .y=judge_y, .fade=hit_fade});
        } else {
            tex.draw_texture(HIT_EFFECT::HIT_EFFECT_OK, {.x=judge_x, .y=judge_y, .fade=fade});
            tex.draw_texture(HIT_EFFECT::OUTER_OK, {.frame=index, .x=judge_x, .y=judge_y, .fade=hit_fade});
        }
        tex.draw_texture(HIT_EFFECT::JUDGE_OK, {.x=judge_x, .y=y + judge_y, .fade=fade});
    }
    else if (type == Judgments::BAD) {
        tex.draw_texture(HIT_EFFECT::JUDGE_BAD, {.x=judge_x, .y=y + judge_y, .fade=fade});
    }
}

bool Judgment::is_finished() const {
    return fade_animation_2->is_finished;
}
