#include "transition.h"
#include "../../libs/global_data.h"
#include <algorithm>

Transition::Transition(const std::string& title, const std::string& subtitle, bool is_second) :
    is_second(is_second) {
    rainbow_up = (MoveAnimation*)global_tex.get_animation(0);
    mini_up = (MoveAnimation*)global_tex.get_animation(1);
    chara_down = (MoveAnimation*)global_tex.get_animation(2);
    song_info_fade = (FadeAnimation*)global_tex.get_animation(3);
    song_info_fade_out = (FadeAnimation*)global_tex.get_animation(4);

    this->title = std::make_unique<OutlinedText>(title, global_tex.skin_config[SC::TRANSITION_TITLE].font_size, ray::WHITE, ray::BLACK, false, 5);
    this->subtitle = std::make_unique<OutlinedText>(subtitle, global_tex.skin_config[SC::TRANSITION_SUBTITLE].font_size, ray::WHITE, ray::BLACK, false, 5);

    if (!load("SongTransition", "transition", title, subtitle, is_second)) return;
    fn_update    = lua_object["update"];
    fn_draw_bg   = lua_object["draw_bg"];
    fn_draw_info = lua_object["draw_info"];
}

Transition::~Transition() {
    if (loading_graphic.has_value()) {
        ray::UnloadTexture(loading_graphic.value());
    }
}

void Transition::add_loading_graphic(const std::string& path) {
    loading_graphic.emplace(ray::LoadTexture(path.c_str()));
    ray::GenTextureMipmaps(&loading_graphic.value());
    ray::SetTextureFilter(loading_graphic.value(), ray::TEXTURE_FILTER_TRILINEAR);
}

void Transition::set_dan(int color, const std::string& rank_name) {
    dan_color = color;
    global_tex.load_folder("dan_loading", "loading_dan");
    if (!rank_name.empty()) {
        dan_rank_text = std::make_unique<OutlinedText>(
            rank_name, global_tex.skin_config[SC::DAN_TITLE].font_size,
            ray::WHITE, ray::BLACK, true);
    }
}

void Transition::draw_dan(float /*total_offset*/) {
    const double f = 36.0 + (get_current_ms() - dan_start_ms) * 0.06;
    const float  y = -106.0f - 0.625f * (float)std::clamp(f - 36.0, 0.0, 179.0);
    const float  a = (float)std::clamp((f - 44.0) / 20.0, 0.0, 1.0);
    const float  black = (float)std::clamp(1.0 - (f - 36.0) / 29.0, 0.0, 1.0);
    const float  dy = -(float)rainbow_up->attribute;

    global_tex.draw_texture(LOADING_DAN::NIGHT, {.y = y + dy});
    if (a > 0.0f) {
        global_tex.draw_texture(LOADING_DAN::PLAQUE,
                                {.frame = std::clamp(dan_color, 0, 6), .y = dy, .fade = a});
        if (dan_rank_text) {
            const SkinInfo* p = global_tex.skin_entry("dan_loading_rank");
            const float rx = p ? p->x : 1697.0f;
            const float ry = p ? p->y : 290.0f;
            dan_rank_text->draw({.x = rx - dan_rank_text->width / 2.0f,
                                 .y = ry - dan_rank_text->height / 2.0f + dy,
                                 .fade = a});
        }
    }
    if (black > 0.0f)
        ray::DrawRectangle(0, 0, global_tex.screen_width, global_tex.screen_height,
                           ray::Fade(ray::BLACK, black));
}

void Transition::start() {
    dan_start_ms = get_current_ms();
    rainbow_up->start();
    mini_up->start();
    chara_down->start();
    song_info_fade->start();
    song_info_fade_out->start();
}

void Transition::update(double current_ms) {
    call(fn_update, "SongTransition:update", current_ms,
         (double)rainbow_up->attribute, (double)song_info_fade->attribute);
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
        offset = global_tex.skin_config[SC::TRANSITION_OFFSET].y - rainbow_up->attribute;
    }
    global_tex.draw_texture(RAINBOW_TRANSITION::TEXT_BG, {.y=(float)-rainbow_up->attribute - offset, .fade=fade_2});

    float x = (float)global_tex.screen_width/2 - title->width/2;
    float y = global_tex.skin_config[SC::TRANSITION_TITLE].y - title->height/2 - rainbow_up->attribute - offset;
    title->draw({.x = x, .y = y, .fade = fade_1});

    x = (float)global_tex.screen_width/2 - subtitle->width/2;
    y = global_tex.skin_config[SC::TRANSITION_SUBTITLE].y - subtitle->height/2 - rainbow_up->attribute - offset;
    subtitle->draw({.x = x, .y = y, .fade = fade_1});
}

void Transition::draw_default(float total_offset) {
    global_tex.draw_texture(RAINBOW_TRANSITION::RAINBOW_BG_BOTTOM, {.y=(float)-rainbow_up->attribute - total_offset});
    global_tex.draw_texture(RAINBOW_TRANSITION::RAINBOW_BG_TOP, {.y=(float)-rainbow_up->attribute - total_offset});
    global_tex.draw_texture(RAINBOW_TRANSITION::RAINBOW_BG, {.y=(float)-rainbow_up->attribute - total_offset});
    float offset = chara_down->attribute;
    float chara_offset = 0;
    if (is_second) {
        offset = chara_down->attribute - mini_up->attribute/3;
        chara_offset = global_tex.skin_config[SC::TRANSITION_CHARA_OFFSET].y;
    }
    global_tex.draw_texture(RAINBOW_TRANSITION::CHARA_LEFT, {.x=(float)-mini_up->attribute/2 - chara_offset, .y=(float)-mini_up->attribute + offset - total_offset});
    global_tex.draw_texture(RAINBOW_TRANSITION::CHARA_RIGHT, {.x=(float)mini_up->attribute/2 + chara_offset, .y=(float)-mini_up->attribute + offset - total_offset});
    global_tex.draw_texture(RAINBOW_TRANSITION::CHARA_CENTER, {.y=(float)-rainbow_up->attribute + offset - total_offset});
}

void Transition::draw() {
    float total_offset = 0;
    if (is_second) total_offset = global_tex.skin_config[SC::TRANSITION_OFFSET].y;
    if (dan_color >= 0) {
        draw_dan(total_offset);
        return;
    }

    const bool scripted_bg = fn_draw_bg.valid();
    if (scripted_bg) {
        call(fn_draw_bg, "SongTransition:draw_bg", total_offset,
             (double)rainbow_up->attribute, (double)mini_up->attribute,
             (double)chara_down->attribute);
        if (!loading_graphic.has_value()) {
            if (fn_draw_info.valid()) {
                call(fn_draw_info, "SongTransition:draw_info", total_offset,
                     (double)rainbow_up->attribute, (double)song_info_fade->attribute,
                     (double)song_info_fade_out->attribute);
            } else {
                draw_song_info();
            }
            return;
        }
    }
    if (loading_graphic.has_value()) {
        ray::Rectangle src = {0, 0, (float)loading_graphic.value().width, (float)loading_graphic.value().height};
        ray::Rectangle dst = {0, global_tex.screen_height + (global_tex.skin_config[SC::TRANSITION_OFFSET].y - global_tex.screen_height) - (float)rainbow_up->attribute - total_offset, (float)global_tex.screen_width, (float)global_tex.screen_height};
        ray::DrawTexturePro(loading_graphic.value(), src, dst, {0,0}, 0, ray::WHITE);
    } else {
        draw_default(total_offset);
    }

    if (fn_draw_info.valid()) {
        call(fn_draw_info, "SongTransition:draw_info", total_offset,
             (double)rainbow_up->attribute, (double)song_info_fade->attribute,
             (double)song_info_fade_out->attribute);
    } else {
        draw_song_info();
    }
}
