#include "gogo_time.h"
#include "../../libs/texture.h"

GogoTime::GogoTime() {
    fire_resize = (TextureResizeAnimation*)tex.get_animation(24, true);

    int change_anim = 25;
    if (const SkinInfo* a = tex.skin_entry("gogo_fire_anim"); a && a->x > 0 && tex.has_animation((int)a->x))
        change_anim = (int)a->x;
    fire_change = (TextureChangeAnimation*)tex.get_animation(change_anim, true);
    fire_fade = 0.5f;
    if (const SkinInfo* f = tex.skin_entry("gogo_fire_fade"); f && f->x > 0)
        fire_fade = f->x;

    fire_resize->start();
    fire_change->start();
}

void GogoTime::update(double current_ms) {
    fire_resize->update(current_ms);
    fire_change->update(current_ms);
}

void GogoTime::draw(float judge_x, float judge_y) {
    tex.draw_texture(GOGO_TIME::FIRE, {
        .frame = (int)fire_change->attribute,
        .scale = (float)(fire_resize->attribute),
        .center = true,
        .x = judge_x,
        .y = judge_y,
        .fade = fire_fade});
}
