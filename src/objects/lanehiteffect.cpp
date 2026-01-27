#include "lanehiteffect.h"

LaneHitEffect::LaneHitEffect(DrumType type, Judgments judgment, bool is_2p)
            : type(type), judgment(judgment), is_2p(is_2p) {
    fade = (FadeAnimation*)tex.get_animation(0, true);
    fade->start();
}

void LaneHitEffect::update(double current_ms) {
    fade->update(current_ms);
}

void LaneHitEffect::draw() {
    if (type == DrumType::DON) {
        tex.draw_texture("lane", "lane_hit_effect", {.frame=0, .fade=fade->attribute, .index=is_2p});
    } else if (type == DrumType::KAT) {
        tex.draw_texture("lane", "lane_hit_effect", {.frame=1, .fade=fade->attribute, .index=is_2p});
    }
    if (judgment == Judgments::GOOD || judgment == Judgments::OK) {
        tex.draw_texture("lane", "lane_hit_effect", {.frame=2, .fade=fade->attribute, .index=is_2p});
    }

}

bool LaneHitEffect::is_finished() const {
    return fade->is_finished;
}
