#include "gauge_hit_effect.h"

GaugeHitEffect::GaugeHitEffect(NoteType note_type, bool is_big)
            : note_type(note_type), is_big(is_big) {
    texture_change = (TextureChangeAnimation*)tex.get_animation(2, true);
    circle_fadein = (FadeAnimation*)tex.get_animation(31, true);
    resize = (TextureResizeAnimation*)tex.get_animation(32, true);
    fade_out = (FadeAnimation*)tex.get_animation(33, true);
    rotation = (FadeAnimation*)tex.get_animation(34, true);

    texture_change->start();
    circle_fadein->start();
    resize->start();
    fade_out->start();
    rotation->start();

    color = ray::Fade(ray::YELLOW, circle_fadein->attribute);

    width = tex.textures["gauge"]["hit_effect"]->width;

    dest_width = width * tex.screen_scale;
    dest_height = width * tex.screen_scale;
    origin = {width/2, width/2};
    rotation_angle = 0;
    x2_pos = -width;
    y2_pos = -width;
}

void GaugeHitEffect::update(double current_ms) {
    texture_change->update(current_ms);
    circle_fadein->update(current_ms);
    resize->update(current_ms);
    fade_out->update(current_ms);
    rotation->update(current_ms);

    if (resize->attribute <= 0.70) {
        color = ray::WHITE;
    } else if (resize->attribute <= 0.80) {
        color = ray::YELLOW;
    } else if (resize->attribute <= 0.90) {
        color = ray::ORANGE;
    } else if (resize->attribute <= 1.00) {
        color = ray::RED;
    }

    dest_width = width * resize->attribute;
    dest_height = width * resize->attribute;
    origin = {dest_width / 2, dest_height / 2};
    x2_pos = -width + (width * resize->attribute);
    y2_pos = -width + (width * resize->attribute);

    rotation_angle = rotation->attribute * 100;

}

void GaugeHitEffect::draw(float y) {
    //Main hit effect texture
    tex.draw_texture("gauge", "hit_effect",
                    {.color=ray::Fade(color, fade_out->attribute),
                    .frame=(int)texture_change->attribute,
                    .center=true,
                    .y=y,
                    .x2=x2_pos,
                    .y2=y2_pos,
                    .origin=origin,
                    .rotation=rotation_angle});

    //Note type texture
    SkinInfo pos_data = tex.skin_config["gauge_hit_effect_note"];
    tex.draw_texture("notes", std::to_string((int)note_type),
        {.x=pos_data.x, .y=y+pos_data.y, .fade=fade_out->attribute});

    //Circle effect texture
    ray::Color texture_color;
    if (circle_fadein->is_finished) {
        texture_color = ray::Fade(ray::WHITE, circle_fadein->attribute);
    } else {
        texture_color = ray::Fade(ray::YELLOW, circle_fadein->attribute);
    }
    if (is_big) {
        tex.draw_texture("gauge", "hit_effect_circle_big", {.color=texture_color, .y=y});
    } else {
        tex.draw_texture("gauge", "hit_effect_circle", {.color=texture_color, .y=y});
    }
}

bool GaugeHitEffect::is_finished() const {
    return fade_out->is_finished;
}
