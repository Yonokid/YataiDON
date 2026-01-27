#include "drumhiteffect.h"

DrumHitEffect::DrumHitEffect(DrumType type, Side side, bool is_2p)
            : type(type), side(side), is_2p(is_2p) {
    fade = (FadeAnimation*)tex.get_animation(1, true);
    fade->start();
}

void DrumHitEffect::update(double current_ms) {
    fade->update(current_ms);
}

void DrumHitEffect::draw() {
    if (type == DrumType::DON) {
        if (side == Side::LEFT) {
            tex.draw_texture("lane", "drum_don_l", {.fade=fade->attribute, .index=is_2p});
        } else if (side == Side::RIGHT) {
            tex.draw_texture("lane", "drum_don_r", {.fade=fade->attribute, .index=is_2p});
        }
    } else if (type == DrumType::KAT) {
        if (side == Side::LEFT) {
            tex.draw_texture("lane", "drum_kat_l", {.fade=fade->attribute, .index=is_2p});
        } else if (side == Side::RIGHT) {
            tex.draw_texture("lane", "drum_kat_r", {.fade=fade->attribute, .index=is_2p});
        }
    }
}

bool DrumHitEffect::is_finished() const {
    return fade->is_finished;
}
