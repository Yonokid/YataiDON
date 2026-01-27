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
    int frame;
    if (type == DrumType::DON) {
        frame = 0;
    } else if (type == DrumType::KAT) {
        frame = 1;
    } else if (judgment == Judgments::GOOD || judgment == Judgments::OK) {
        frame = 2;
    }
    tex.draw_texture("lane", "lane_hit_effect", {.frame=frame, .fade=(float)fade->attribute, .index=is_2p});
}

bool LaneHitEffect::is_finished() const {
    return fade->is_finished;
}
