#include "gogo_time.h"

GogoTime::GogoTime(bool is_2p) : is_2p(is_2p) {
    explosion_anim = (TextureChangeAnimation*)tex.get_animation(23, true);
    fire_resize = (TextureResizeAnimation*)tex.get_animation(24, true);
    fire_change = (TextureChangeAnimation*)tex.get_animation(25, true);

    explosion_anim->start();
    fire_resize->start();
    fire_change->start();
}

void GogoTime::update(double current_ms) {
    explosion_anim->update(current_ms);
    fire_resize->update(current_ms);
    fire_change->update(current_ms);
}

void GogoTime::draw(float judge_x, float judge_y) {
    tex.draw_texture("gogo_time", "fire", {
        .frame = (int)fire_change->attribute,
        .scale = (float)(fire_resize->attribute),
        .center = true,
        .x = judge_x,
        .y = judge_y,
        .fade = 0.5f,
        .index = (int)is_2p
    });

    if (!explosion_anim->is_finished && !is_2p) {
        for (int i = 0; i < 5; i++) {
            tex.draw_texture("gogo_time", "explosion", {
                .frame = (int)explosion_anim->attribute,
                .index = i
            });
        }
    }
}
