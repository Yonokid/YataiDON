#include "drum_hit_effect.h"

DrumHitEffect::DrumHitEffect(DrumType type, Side side)
            : type(type), side(side) {
    fade = (FadeAnimation*)tex.get_animation(1, true);
    fade->start();
}

void DrumHitEffect::update(double current_ms) {
    fade->update(current_ms);
}

void DrumHitEffect::draw(float y) {
    if (type == DrumType::DON) {
        if (side == Side::LEFT) {
            tex.draw_texture(LANE::DRUM_DON_L, {.y=y, .fade=fade->attribute});
        } else if (side == Side::RIGHT) {
            tex.draw_texture(LANE::DRUM_DON_R, {.y=y, .fade=fade->attribute});
        }
    } else if (type == DrumType::KAT) {
        if (side == Side::LEFT) {
            tex.draw_texture(LANE::DRUM_KAT_L, {.y=y, .fade=fade->attribute});
        } else if (side == Side::RIGHT) {
            tex.draw_texture(LANE::DRUM_KAT_R, {.y=y, .fade=fade->attribute});
        }
    }
}

bool DrumHitEffect::is_finished() const {
    return fade->is_finished;
}
