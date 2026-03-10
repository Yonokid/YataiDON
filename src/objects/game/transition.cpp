#include "transition.h"

Transition::Transition(const std::string& title, const std::string& subtitle, bool is_second) :
    is_second(is_second) {
    rainbow_up = (MoveAnimation*)global_tex.get_animation(0);
    mini_up = (MoveAnimation*)global_tex.get_animation(1);
    chara_down = (MoveAnimation*)global_tex.get_animation(2);
    song_info_fade = (FadeAnimation*)global_tex.get_animation(3);
    song_info_fade_out = (FadeAnimation*)global_tex.get_animation(4);

    this->title = new OutlinedText(title, global_tex.skin_config["transition_title"].font_size, ray::WHITE, ray::BLACK, false, 5);
    this->subtitle = new OutlinedText(subtitle, global_tex.skin_config["transition_subtitle"].font_size, ray::WHITE, ray::BLACK, false, 5);
}

void Transition::start() {
    rainbow_up->start();
    mini_up->start();
    chara_down->start();
    song_info_fade->start();
    song_info_fade_out->start();
}

void Transition::update(double current_ms) {
    rainbow_up->update(current_ms);
    chara_down->update(current_ms);
    mini_up->update(current_ms);
    song_info_fade->update(current_ms);
    song_info_fade_out->update(current_ms);
}

bool Transition::is_finished() {
    return song_info_fade->is_finished;
}

void Transition::draw_song_info() {
    float fade_1 = song_info_fade->attribute;
    float fade_2 = std::min(0.70, song_info_fade->attribute);
    float offset = 0;
    if (is_second) {
        fade_1 = song_info_fade_out->attribute;
        fade_2 = std::min(0.70, song_info_fade_out->attribute);
        offset = global_tex.skin_config["transition_offset"].y - rainbow_up->attribute;
    }
    global_tex.draw_texture("rainbow_transition", "text_bg", {.y=(float)-rainbow_up->attribute - offset, .fade=fade_2});

    float x = (float)global_tex.screen_width/2 - title->width/2;
    float y = global_tex.skin_config["transition_title"].y - title->height/2 - rainbow_up->attribute - offset;
    title->draw({.x = x, .y = y, .fade = fade_1});

    x = (float)global_tex.screen_width/2 - subtitle->width/2;
    y = global_tex.skin_config["transition_subtitle"].y - subtitle->height/2 - rainbow_up->attribute - offset;
    subtitle->draw({.x = x, .y = y, .fade = fade_1});
}

void Transition::draw() {
    float total_offset = 0;
    if (is_second) total_offset = global_tex.skin_config["transition_offset"].y;
    global_tex.draw_texture("rainbow_transition", "rainbow_bg_bottom", {.y=(float)-rainbow_up->attribute - total_offset});
    global_tex.draw_texture("rainbow_transition", "rainbow_bg_top", {.y=(float)-rainbow_up->attribute - total_offset});
    global_tex.draw_texture("rainbow_transition", "rainbow_bg", {.y=(float)-rainbow_up->attribute - total_offset});
    float offset = chara_down->attribute;
    float chara_offset = 0;
    if (is_second) {
        offset = chara_down->attribute - mini_up->attribute/3;
        chara_offset = global_tex.skin_config["transition_chara_offset"].y;
    }
    global_tex.draw_texture("rainbow_transition", "chara_left", {.x=(float)-mini_up->attribute/2 - chara_offset, .y=(float)-mini_up->attribute + offset - total_offset});
    global_tex.draw_texture("rainbow_transition", "chara_right", {.x=(float)mini_up->attribute/2 + chara_offset, .y=(float)-mini_up->attribute + offset - total_offset});
    global_tex.draw_texture("rainbow_transition", "chara_center", {.y=(float)-rainbow_up->attribute + offset - total_offset});

    draw_song_info();
}
