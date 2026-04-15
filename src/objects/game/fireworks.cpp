#include "fireworks.h"

Fireworks::Fireworks() {
    explosion_anim = (TextureChangeAnimation*)tex.get_animation(23, true);

    explosion_anim->start();
}

void Fireworks::update(double current_ms) {
    explosion_anim->update(current_ms);
}

void Fireworks::draw() {
    if (!explosion_anim->is_finished) {
        for (int i = 0; i < 5; i++) {
            tex.draw_texture(GOGO_TIME::EXPLOSION, {.frame = (int)explosion_anim->attribute, .index = i});
        }
    }
}

bool Fireworks::is_finished() {
    return explosion_anim->is_finished;
}
