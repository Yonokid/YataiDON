#include "gogo_time.h"
#include "../../libs/texture.h"

GogoTime::GogoTime() {
    fire_resize = (TextureResizeAnimation*)tex.get_animation(24, true);
    fire_change = (TextureChangeAnimation*)tex.get_animation(25, true);

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
        .fade = 0.5f});
}
