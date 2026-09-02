#include "fireworks.h"
#include "../../libs/texture.h"

Fireworks::Fireworks() {
    explosion_anim = (TextureChangeAnimation*)tex.get_animation(23, true);

    explosion_anim->start();
}

void Fireworks::update(double current_ms) {
    explosion_anim->update(current_ms);
}

void Fireworks::draw() {
    if (!explosion_anim->is_finished) {
        int slots = 5;
        int mirror_from = -1;
        if (const SkinInfo* s = tex.skin_entry("gogo_explosion_slots"); s && s->x > 0) {
            slots = (int)s->x;
            if (s->y > 0) mirror_from = (int)s->y;
        }
        for (int i = 0; i < slots; i++) {
            tex.draw_texture(GOGO_TIME::EXPLOSION, {
                .frame = (int)explosion_anim->attribute,
                .mirror = (mirror_from >= 0 && i >= mirror_from) ? Mirror::HORIZONTAL : Mirror::NONE,
                .index = i});
        }
    }
}

bool Fireworks::is_finished() {
    return explosion_anim->is_finished;
}
