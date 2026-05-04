#include "gauge.h"
#include "../../libs/texture.h"

Gauge::Gauge(PlayerNum player_num, int difficulty, int level, int total_notes)
    : player_num(player_num), difficulty(std::min((int)Difficulty::ONI, difficulty)),
      level(std::min(10, level)), total_notes(total_notes),
      gauge_length(0), previous_length(0), is_clear(false), is_rainbow(false) {

    clear_start = {52, 60, 69, 69};
    gauge_max = 87.0f;

    tamashii_fire_change = (TextureChangeAnimation*)tex.get_animation(25);

    if (this->difficulty == (int)Difficulty::HARD) {
        string_diff = "_hard";
    } else if (this->difficulty == (int)Difficulty::NORMAL) {
        string_diff = "_normal";
    } else if (this->difficulty == (int)Difficulty::EASY) {
        string_diff = "_easy";
    } else {
        string_diff = "_hard";
    }

    // Initialize the gauge table
    table = {
        {
            {36.0f, 0.75f, -0.5f},
            {38.0f, 0.75f, -0.5f},
            {38.0f, 0.75f, -0.5f},
            {44.0f, 0.75f, -0.5f},
            {44.0f, 0.75f, -0.5f},
        },
        {
            {45.939f, 0.75f, -0.5f},
            {45.939f, 0.75f, -0.5f},
            {48.676f, 0.75f, -0.5f},
            {49.232f, 0.75f, -0.75f},
            {52.5f, 0.75f, -1.0f},
            {52.5f, 0.75f, -1.0f},
            {52.5f, 0.75f, -1.0f},
        },
        {
            {54.325f, 0.75f, -0.75f},
            {54.325f, 0.75f, -0.75f},
            {50.774f, 0.75f, -1.0f},
            {48.410f, 0.75f, -1.17f},
            {47.246f, 0.75f, -1.25f},
            {48.120f, 0.75f, -1.25f},
            {48.120f, 0.75f, -1.25f},
            {48.120f, 0.75f, -1.25f},
        },
        {
            {56.603f, 0.5f, -1.6f},
            {56.603f, 0.5f, -1.6f},
            {56.603f, 0.5f, -1.6f},
            {56.603f, 0.5f, -1.6f},
            {56.603f, 0.5f, -1.6f},
            {56.603f, 0.5f, -1.6f},
            {56.603f, 0.5f, -1.6f},
            {56.0f, 0.5f, -2.0f},
            {61.428f, 0.5f, -2.0f},
            {61.428f, 0.5f, -2.0f},
        }
    };

    gauge_update_anim = (FadeAnimation*)tex.get_animation(10);
    rainbow_animation = (TextureChangeAnimation*)tex.get_animation(64);
}

float Gauge::get_progress() {
    return gauge_length / gauge_max;
}

void Gauge::add_good() {
    gauge_update_anim->start();
    previous_length = (int)gauge_length;
    gauge_length += (1.0f / total_notes) * (100.0f * (clear_start[difficulty] / table[difficulty][level - 1].clear_rate));
    if (gauge_length > gauge_max) {
        gauge_length = gauge_max;
    }
}

void Gauge::add_ok() {
    gauge_update_anim->start();
    previous_length = (int)gauge_length;
    gauge_length += ((1.0f * table[difficulty][level - 1].ok_multiplier) / total_notes) *
                     (100.0f * (clear_start[difficulty] / table[difficulty][level - 1].clear_rate));
    if (gauge_length > gauge_max) {
        gauge_length = gauge_max;
    }
}

void Gauge::add_bad() {
    previous_length = (int)gauge_length;
    gauge_length += ((1.0f * table[difficulty][level - 1].bad_multiplier) / total_notes) *
                     (100.0f * (clear_start[difficulty] / table[difficulty][level - 1].clear_rate));
    if (gauge_length < 0) {
        gauge_length = 0;
    }
    if (previous_length == gauge_max && gauge_length < gauge_max) {
        if (rainbow_fade_in.has_value()) rainbow_fade_in.reset();
    }
}

void Gauge::update(double current_ms) {
    is_clear = gauge_length > clear_start[std::min(difficulty, (int)Difficulty::HARD)] - 1;
    is_rainbow = gauge_length == gauge_max;

    if (gauge_length == gauge_max && !rainbow_fade_in.has_value()) {
        rainbow_fade_in = (FadeAnimation*)tex.get_animation(63);
        rainbow_fade_in.value()->start();
    }

    gauge_update_anim->update(current_ms);
    tamashii_fire_change->update(current_ms);

    if (rainbow_fade_in.has_value()) {
        rainbow_fade_in.value()->update(current_ms);
    }

    rainbow_animation->update(current_ms);
}

void Gauge::draw(float y) {
    bool mirrored = y > tex.screen_height / 2.0f;
    std::string mirror = mirrored ? "vertical" : "";

    tex.draw_texture(tex_id_map.at("gauge/border" + string_diff), {.mirror = mirror, .y = y});

    tex.draw_texture(tex_id_map.at("gauge/" + (std::to_string((int)player_num) + "p_unfilled" + string_diff)), {.mirror = mirror, .y = y, .index=mirrored});

    int gauge_length_int = (int)gauge_length;
    int clear_point = clear_start[difficulty];
    float bar_width = tex.textures[tex_id_map.at("gauge/" + std::to_string((int)player_num) + "p_bar")]->width;

    tex.draw_texture(tex_id_map.at("gauge/" + (std::to_string((int)player_num) + "p_bar")), {.y = y, .x2 = std::min(gauge_length_int * bar_width, (clear_point - 1) * bar_width) - bar_width, .index=mirrored});

    if (gauge_length_int >= clear_point - 1) {
        tex.draw_texture(GAUGE::BAR_CLEAR_TRANSITION, {.mirror = mirror, .x = (clear_point - 1) * bar_width, .y = y, .index=mirrored});
    }

    if (gauge_length_int > clear_point) {
        tex.draw_texture(GAUGE::BAR_CLEAR_TOP, {.mirror = mirror, .x = clear_point * bar_width, .y = y, .x2 = (gauge_length_int - clear_point) * bar_width, .index=mirrored});

        tex.draw_texture(GAUGE::BAR_CLEAR_BOTTOM, {.x = clear_point * bar_width, .y = y, .x2 = (gauge_length_int - clear_point) * bar_width, .index=mirrored});
    }

    // Rainbow effect for full gauge
    if (gauge_length_int == gauge_max && rainbow_fade_in.has_value()) {
        if (0 < rainbow_animation->attribute && rainbow_animation->attribute < 8) {
            tex.draw_texture(tex_id_map.at("gauge/rainbow" + string_diff), {
                .frame = (int)rainbow_animation->attribute - 1,
                .mirror = mirror,
                .y = y,
                .fade = rainbow_fade_in.value()->attribute,
                .index = mirrored,
            });
        }
        tex.draw_texture(tex_id_map.at("gauge/rainbow" + string_diff), {
            .frame = (int)rainbow_animation->attribute,
            .mirror = mirror,
            .y = y,
            .fade = std::min(rainbow_fade_in.value()->attribute, 0.75),
            .index = mirrored,
        });
    }

    if (gauge_length_int <= gauge_max && gauge_length_int > previous_length) {
        if (gauge_length_int == clear_start[difficulty]) {
            tex.draw_texture(GAUGE::BAR_CLEAR_TRANSITION_FADE, {
                .mirror = mirror,
                .x = gauge_length_int * bar_width,
                .y = y,
                .fade = gauge_update_anim->attribute,
                .index = mirrored,
            });
        } else if (gauge_length_int > clear_start[difficulty]) {
            tex.draw_texture(GAUGE::BAR_CLEAR_FADE, {
                .x = gauge_length_int * bar_width,
                .y = y,
                .fade = gauge_update_anim->attribute,
                .index = mirrored,
            });
        } else {
            tex.draw_texture(tex_id_map.at("gauge/" + (std::to_string((int)player_num) + "p_bar_fade")), {
                .x = gauge_length_int * bar_width,
                .y = y,
                .fade = gauge_update_anim->attribute,
                .index = mirrored,
            });
        }
    }

    tex.draw_texture(tex_id_map.at("gauge/overlay" + string_diff), {
        .mirror = mirror,
        .y = y,
        .fade = 0.15f,
        .index = mirrored,
    });

    // Draw clear status indicators
    if (gauge_length_int >= clear_point - 1) {
        tex.draw_texture(GAUGE::CLEAR, {
            .y = y,
            .index = std::min(2, difficulty) + (mirrored * 3)
        });

        if (is_rainbow) {
            tex.draw_texture(GAUGE::TAMASHII_FIRE, {
                .frame = (int)tamashii_fire_change->attribute,
                .scale = 0.75f,
                .center = true,
                .y = y,
                .index = mirrored,
            });
        }

        tex.draw_texture(GAUGE::TAMASHII, {.y = y, .index = mirrored});

        int fire_frame = (int)tamashii_fire_change->attribute;
        if (is_rainbow && (fire_frame == 0 || fire_frame == 1 || fire_frame == 4 || fire_frame == 5)) {
            tex.draw_texture(GAUGE::TAMASHII_OVERLAY, {.y = y, .fade = 0.5f, .index = mirrored});
        }
    } else {
        tex.draw_texture(GAUGE::CLEAR_DARK, {.y = y, .index = std::min(2, difficulty) + (mirrored * 3)});

        tex.draw_texture(GAUGE::TAMASHII_DARK, {.y = y, .index = mirrored});
    }
}
