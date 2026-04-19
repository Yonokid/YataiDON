#include "gauge.h"

ResultGauge::ResultGauge(PlayerNum player_num, int gauge_length, bool is_2p)
: player_num(player_num), gauge_length(gauge_length), is_2p(is_2p) {
    difficulty = std::min(Difficulty::HARD, Difficulty(global_data.session_data[(int)player_num].selected_difficulty));
    clear_start = {69, 69, 69};
    gauge_max = 87;
    if (difficulty >= Difficulty::HARD) {
        string_diff = "_hard";
    }
    else if (difficulty >= Difficulty::NORMAL) {
        string_diff = "_normal";
    } else if (difficulty >= Difficulty::EASY) {
        string_diff = "_easy";
    }
    rainbow_animation = (TextureChangeAnimation*)tex.get_animation(16);
    gauge_fade_in = (FadeAnimation*)tex.get_animation(17);
    tamashii_fire_change = (TextureChangeAnimation*)tex.get_animation(20);
    rainbow_animation->start();
    gauge_fade_in->start();

    if (gauge_length == gauge_max) {
        state = ResultState::RAINBOW;
    } else if (gauge_length >= clear_start[(int)difficulty] - 1) {
        state = ResultState::CLEAR;
    } else {
        state = ResultState::FAIL;
    }
}

void ResultGauge::update(double current_ms) {
    rainbow_animation->update(current_ms);
    tamashii_fire_change->update(current_ms);
    if (rainbow_animation->is_finished) {
        rainbow_animation->restart();
    }
    gauge_fade_in->update(current_ms);
}

void ResultGauge::draw() {
    float scale = 10.0/11.0;
    std::string player_str = std::to_string(static_cast<int>(player_num)) + "p";
    tex.draw_texture(tex.get_enum("gauge/" + (player_str + "_unfilled" + string_diff)), {.scale=scale, .fade=gauge_fade_in->attribute, .index=is_2p});
    float bar_width = tex.textures[tex.get_enum("gauge/" + player_str + "_bar")]->width;
    if (gauge_length == gauge_max) {
        if (0 < rainbow_animation->attribute && rainbow_animation->attribute < 8) {
            tex.draw_texture(tex.get_enum("gauge/rainbow" + string_diff), {.frame=(int)rainbow_animation->attribute-1, .scale=scale, .fade=gauge_fade_in->attribute, .index=is_2p});
        }
        tex.draw_texture(tex.get_enum("gauge/rainbow" + string_diff), {.frame=(int)rainbow_animation->attribute, .scale=scale, .fade=gauge_fade_in->attribute, .index=is_2p});
    } else {
        tex.draw_texture(tex.get_enum("gauge/" + (player_str + "_bar")), {.scale=scale, .x2=(gauge_length*bar_width)-(bar_width*6), .fade=gauge_fade_in->attribute, .index=is_2p});
        if (gauge_length >= clear_start[(int)difficulty] - 1) {
            tex.draw_texture(tex.get_enum("gauge/" + (player_str + "bar_clear_transition")), {.scale=scale, .x=(clear_start[(int)difficulty]*bar_width)-(bar_width*7), .fade=gauge_fade_in->attribute, .index=is_2p});
            tex.draw_texture(tex.get_enum("gauge/" + (player_str + "bar_clear_top")), {.scale=scale, .x=(clear_start[(int)difficulty]*bar_width)-(bar_width*6), .x2=(gauge_length - clear_start[(int)difficulty])*bar_width, .fade=gauge_fade_in->attribute, .index=is_2p});
            tex.draw_texture(tex.get_enum("gauge/" + (player_str + "bar_clear_bottom")), {.scale=scale, .x=(clear_start[(int)difficulty]*bar_width)-(bar_width*6), .x2=(gauge_length - clear_start[(int)difficulty])*bar_width, .fade=gauge_fade_in->attribute, .index=is_2p});
        }
    }
    tex.draw_texture(tex.get_enum("gauge/overlay" + string_diff), {.scale=scale, .fade=std::min(0.15, gauge_fade_in->attribute), .index=is_2p});
    tex.draw_texture(GAUGE::FOOTER, {.scale=scale, .fade=gauge_fade_in->attribute, .index=is_2p});

    if (gauge_length >= clear_start[(int)difficulty] - 1) {
        tex.draw_texture(GAUGE::CLEAR, {.scale=scale, .fade=gauge_fade_in->attribute, .index=(int)difficulty+(is_2p*3)});
        if (state == ResultState::RAINBOW) {
            tex.draw_texture(GAUGE::TAMASHII_FIRE, {.frame=(int)tamashii_fire_change->attribute, .scale=(float)0.75 * scale, .center=true, .fade=gauge_fade_in->attribute, .index=is_2p});
        }
        tex.draw_texture(GAUGE::TAMASHII, {.scale=scale, .fade=gauge_fade_in->attribute, .index=is_2p});
        int a = tamashii_fire_change->attribute;
        if (state == ResultState::RAINBOW && (a == 0 || a == 1 || a == 4 || a == 5)) {
            tex.draw_texture(GAUGE::TAMASHII_OVERLAY, {.scale=scale, .fade=std::min(0.5, gauge_fade_in->attribute), .index=is_2p});
        }
    } else {
        tex.draw_texture(GAUGE::CLEAR_DARK, {.scale=scale, .fade=gauge_fade_in->attribute, .index=(int)difficulty+(is_2p*3)});
        tex.draw_texture(GAUGE::TAMASHII_DARK, {.scale=scale, .fade=gauge_fade_in->attribute, .index=is_2p});
    }
}
