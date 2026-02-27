#include "lane_hit_effect.h"

LaneHitEffect::LaneHitEffect(DrumType type, Judgments judgment)
            : type(type), judgment(judgment) {
    fade = (FadeAnimation*)tex.get_animation(0, true);
    fade->start();
}

void LaneHitEffect::update(double current_ms) {
    fade->update(current_ms);
}

void LaneHitEffect::draw(float y) {
    if (type == DrumType::DON) {
        tex.draw_texture("lane", "lane_hit_effect", {.frame=0, .y=y, .fade=fade->attribute});
    } else if (type == DrumType::KAT) {
        tex.draw_texture("lane", "lane_hit_effect", {.frame=1, .y=y, .fade=fade->attribute});
    }
    if (judgment == Judgments::GOOD || judgment == Judgments::OK) {
        tex.draw_texture("lane", "lane_hit_effect", {.frame=2, .y=y, .fade=fade->attribute});
    }

}

bool LaneHitEffect::is_finished() const {
    return fade->is_finished;
}
