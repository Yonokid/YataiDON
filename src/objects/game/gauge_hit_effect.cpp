#include "gauge_hit_effect.h"

GaugeHitEffect::GaugeHitEffect(NoteType note_type, bool is_big, bool is_2p)
            : note_type(note_type), is_big(is_big), is_2p(is_2p) {
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

    width = tex.textures[GAUGE::HIT_EFFECT]->width;

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
        if (is_2p) {
            color = ray::GREEN;
        } else {
            color = ray::ORANGE;
        }
    } else if (resize->attribute <= 1.00) {
        if (is_2p) {
            color = ray::Color{84, 250, 238, 255};
        } else {
            color = ray::RED;
        }
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
    tex.draw_texture(GAUGE::HIT_EFFECT,
                    {.color=ray::Fade(color, fade_out->attribute),
                    .frame=(int)texture_change->attribute,
                    .center=true,
                    .y=y,
                    .x2=x2_pos,
                    .y2=y2_pos,
                    .origin=origin,
                    .rotation=rotation_angle,
                    .index=is_2p});

    //Note type texture
    SkinInfo pos_data = tex.skin_config[SC::GAUGE_HIT_EFFECT_NOTE];
    tex.draw_texture(tex_id_map.at("notes/" + (std::to_string((int)note_type))),
        {.x=pos_data.x, .y=y+pos_data.y + (pos_data.height * is_2p), .fade=fade_out->attribute});

    //Circle effect texture
    ray::Color texture_color;
    if (circle_fadein->is_finished) {
        texture_color = ray::Fade(ray::WHITE, circle_fadein->attribute);
    } else {
        texture_color = ray::Fade(ray::YELLOW, circle_fadein->attribute);
    }
    if (is_big) {
        tex.draw_texture(GAUGE::HIT_EFFECT_CIRCLE_BIG, {.color=texture_color, .y=y, .index=is_2p});
    } else {
        tex.draw_texture(GAUGE::HIT_EFFECT_CIRCLE, {.color=texture_color, .y=y, .index=is_2p});
    }
}

bool GaugeHitEffect::is_finished() const {
    return fade_out->is_finished;
}
