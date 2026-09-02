#include "gauge.h"
#include "../../libs/texture.h"

Gauge::Gauge(int total_notes, int difficulty, int level, PlayerNum player_num)
    : player_num(player_num) {
    this->difficulty = std::min((int)Difficulty::ONI, difficulty);
    GaugeTable table_row = table[this->difficulty][std::min(9, level - 1)];
    good_points = (int)std::ceil(1000000 / (total_notes * table_row.soul_percent));
    ok_points   = (int)std::round(good_points * table_row.ok_multiplier);
    bad_points  = (int)std::round(good_points * table_row.bad_multiplier);
    points = 0;

    if (this->difficulty == (int)Difficulty::EASY)        string_diff = "_easy";
    else if (this->difficulty == (int)Difficulty::NORMAL) string_diff = "_normal";
    else                                                   string_diff = "_hard";

    tamashii_fire_change = (TextureChangeAnimation*)tex.get_animation(25);
    gauge_update_anim    = (FadeAnimation*)tex.get_animation(10);
}

void Gauge::add_good() {
    if (gauge_update_anim) gauge_update_anim->start();
    previous_points = points;
    points = std::max(0, std::min(max_points, points + good_points));
}

void Gauge::add_ok() {
    if (gauge_update_anim) gauge_update_anim->start();
    previous_points = points;
    points = std::max(0, std::min(max_points, points + ok_points));
}

void Gauge::add_bad() {
    previous_points = points;
    points = std::max(0, std::min(max_points, points + bad_points));

    if (previous_points == max_points && points < max_points) {
        if (rainbow_fade_in.has_value()) rainbow_fade_in.reset();
        rainbow_start_ms = -1.0;
        rainbow_frac     = 0.0f;
    }
}

void Gauge::update(double current_ms) {
    if (get_is_rainbow() && !rainbow_fade_in.has_value()) {
        rainbow_fade_in = (FadeAnimation*)tex.get_animation(63);
        rainbow_fade_in.value()->start();
        rainbow_start_ms = current_ms;
    }

    if (gauge_update_anim)    gauge_update_anim->update(current_ms);
    if (tamashii_fire_change) tamashii_fire_change->update(current_ms);

    if (rainbow_fade_in.has_value()) {
        rainbow_fade_in.value()->update(current_ms);
        rainbow_frac = (float)fmod((current_ms - rainbow_start_ms) / 75.0, 8.0);
    }
}

void Gauge::draw(float y) {
    bool mirrored = y > tex.screen_height / 2.0f;
    Mirror mirror = mirrored ? Mirror::VERTICAL : Mirror::NONE;

    tex.draw_texture(tex.get_enum("gauge/border" + string_diff), {.mirror = mirror, .y = y});

    tex.draw_texture(tex.get_enum("gauge/" + (std::to_string((int)player_num) + "p_unfilled" + string_diff)),
                      {.mirror = mirror, .y = y, .index = mirrored});

    constexpr int bar_units = 87;
    int gauge_length_int = points * bar_units / max_points;
    int previous_length_int = previous_points * bar_units / max_points;
    int clear_point = clear_points * bar_units / max_points;
    float bar_width  = tex.textures[tex.get_enum("gauge/" + std::to_string((int)player_num) + "p_bar")]->width;

    tex.draw_texture(tex.get_enum("gauge/" + (std::to_string((int)player_num) + "p_bar")),
                      {.y = y, .x2 = std::min(gauge_length_int * bar_width, (clear_point - 1) * bar_width) - bar_width, .index = mirrored});

    if (gauge_length_int >= clear_point - 1)
        tex.draw_texture(GAUGE::BAR_CLEAR_TRANSITION,
                          {.mirror = mirror, .x = (clear_point - 1) * bar_width, .y = y, .index = mirrored});

    if (gauge_length_int > clear_point) {
        tex.draw_texture(GAUGE::BAR_CLEAR_TOP,
                          {.mirror = mirror, .x = clear_point * bar_width, .y = y,
                           .x2 = (gauge_length_int - clear_point) * bar_width, .index = mirrored});
        tex.draw_texture(GAUGE::BAR_CLEAR_BOTTOM,
                          {.x = clear_point * bar_width, .y = y,
                           .x2 = (gauge_length_int - clear_point) * bar_width, .index = mirrored});
    }

    if (get_is_rainbow() && rainbow_fade_in.has_value()) {
        float fade    = rainbow_fade_in.value()->attribute;
        int   frame_a = (int)rainbow_frac % 8;
        int   frame_b = (frame_a + 1) % 8;
        float t       = rainbow_frac - (int)rainbow_frac;
        tex.draw_texture(tex.get_enum("gauge/rainbow" + string_diff),
                          {.frame = frame_a, .mirror = mirror, .y = y, .fade = fade, .index = mirrored});
        tex.draw_texture(tex.get_enum("gauge/rainbow" + string_diff),
                          {.frame = frame_b, .mirror = mirror, .y = y, .fade = fade * t, .index = mirrored});
    }

    if (gauge_length_int <= bar_units && gauge_length_int > previous_length_int) {
        if (gauge_length_int == clear_point) {
            tex.draw_texture(GAUGE::BAR_CLEAR_TRANSITION_FADE,
                              {.mirror = mirror, .x = gauge_length_int * bar_width, .y = y,
                               .fade = gauge_update_anim->attribute, .index = mirrored});
        } else if (gauge_length_int > clear_point) {
            tex.draw_texture(GAUGE::BAR_CLEAR_FADE,
                              {.x = gauge_length_int * bar_width, .y = y,
                               .fade = gauge_update_anim->attribute, .index = mirrored});
        } else {
            tex.draw_texture(tex.get_enum("gauge/" + (std::to_string((int)player_num) + "p_bar_fade")),
                              {.x = gauge_length_int * bar_width, .y = y,
                               .fade = gauge_update_anim->attribute, .index = mirrored});
        }
    }

    tex.draw_texture(tex.get_enum("gauge/overlay" + string_diff),
                      {.mirror = mirror, .y = y, .fade = 0.15f, .index = mirrored});

    if (gauge_length_int >= clear_point - 1) {
        tex.draw_texture(tex.get_enum("gauge/clear_" + global_data.config->general.language),
                          {.y = y, .index = std::min(2, difficulty) + (mirrored * 3)});
        if (get_is_rainbow()) {
            tex.draw_texture(GAUGE::TAMASHII_FIRE,
                              {.frame = (int)tamashii_fire_change->attribute, .scale = 0.75f,
                               .center = true, .y = y, .index = mirrored});
        }
        tex.draw_texture(GAUGE::TAMASHII, {.y = y, .index = mirrored});
        int fire_frame = (int)tamashii_fire_change->attribute;
        if (get_is_rainbow() && (fire_frame == 0 || fire_frame == 1 || fire_frame == 4 || fire_frame == 5))
            tex.draw_texture(GAUGE::TAMASHII_OVERLAY, {.y = y, .fade = 0.5f, .index = mirrored});
    } else {
        tex.draw_texture(tex.get_enum("gauge/clear_dark_" + global_data.config->general.language),
                          {.y = y, .index = std::min(2, difficulty) + (mirrored * 3)});
        tex.draw_texture(GAUGE::TAMASHII_DARK, {.y = y, .index = mirrored});
    }
}
