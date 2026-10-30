#include "transition.h"

#include <algorithm>

Transition::Transition(const std::string& title, const std::string& subtitle, bool is_second) : is_second(is_second) {

    this->title = std::make_unique<OutlinedText>(title, global_tex.get_skin_config(SC::TRANSITION_TITLE)->font_size,
                                                 Color::WHITE, Color::BLACK, false, 5);
    this->subtitle = std::make_unique<OutlinedText>(
        subtitle, global_tex.get_skin_config(SC::TRANSITION_SUBTITLE)->font_size, Color::WHITE, Color::BLACK, false, 5);

    t_rainbow_text_bg = global_tex.get_texture("rainbow_transition/text_bg");

    if (!load("SongTransition", "transition", title, subtitle, is_second)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_draw_bg = lua_object["draw_bg"];
    fn_draw_info = lua_object["draw_info"];
    fn_state = lua_object["state"];
    fn_start = lua_object["start"];
}

Transition::~Transition() {
    if (loading_graphic.has_value()) {
        unload_texture(loading_graphic.value());
    }
}

void Transition::add_loading_graphic(const std::string& path) {
    Texture tex = load_texture_mipmapped(path.c_str());
    if (tex.id == 0) {
        spdlog::error("Failed to load transition loading graphic: {}", path);
        return;
    }
    if (loading_graphic.has_value()) {
        unload_texture(loading_graphic.value());
        loading_graphic.reset();
    }
    loading_graphic.emplace(tex);
}

void Transition::set_dan(int color, const std::string& rank_name) {
    dan_color = color;
    global_tex.load_folder("dan_loading", "loading_dan");
    t_loading_dan_night = global_tex.get_texture("loading_dan/night");
    t_loading_dan_plaque = global_tex.get_texture("loading_dan/plaque");
    if (global_tex.has_texture("rainbow_transition/rainbow_bg")) {
        t_rainbow_bg_bottom = global_tex.get_texture("rainbow_transition/rainbow_bg_bottom");
        t_rainbow_bg_top = global_tex.get_texture("rainbow_transition/rainbow_bg_top");
        t_rainbow_bg = global_tex.get_texture("rainbow_transition/rainbow_bg");
    }
    if (!rank_name.empty()) {
        dan_rank_text = std::make_unique<OutlinedText>(
            rank_name, global_tex.get_skin_config(SC::DAN_SELECT_TITLE)->font_size, Color::WHITE, Color::BLACK, true);
    }
}

void Transition::draw_dan(float total_offset) {
    const double f = 36.0 + (get_current_ms() - dan_start_ms) * 0.06;
    const float y = -106.0f - 0.625f * static_cast<float>(std::clamp(f - 36.0, 0.0, 179.0));
    const float a = std::clamp((f - 44.0) / 20.0, 0.0, 1.0);
    const float black = std::clamp(1.0 - (f - 36.0) / 29.0, 0.0, 1.0);
    const float dy = -static_cast<float>(cur_rainbow) - total_offset;

    global_tex.draw_texture(t_loading_dan_night, {.y = y + dy});
    if (a > 0.0f) {
        global_tex.draw_texture(t_loading_dan_plaque, {.frame = std::clamp(dan_color, 0, 6), .y = dy, .fade = a});
        if (dan_rank_text) {
            const SkinInfo* p = global_tex.get_skin_config(SC::DAN_LOADING_RANK);
            const float rx = p ? p->x : 1697.0f;
            const float ry = p ? p->y : 290.0f;
            dan_rank_text->draw(
                {.x = rx - dan_rank_text->width / 2.0f, .y = ry - dan_rank_text->height / 2.0f + dy, .fade = a});
        }
    }
    if (black > 0.0f) {
        draw_rectangle(0, 0, global_tex.screen_width, global_tex.screen_height, fade(Color::BLACK, black));
    }
}

// dan course without loading art: the rainbow backdrop alone, no song info or characters
void Transition::draw_dan_rainbow(float total_offset) {
    const float y = -static_cast<float>(cur_rainbow) - total_offset;
    global_tex.draw_texture(t_rainbow_bg_bottom, {.y = y});
    global_tex.draw_texture(t_rainbow_bg_top, {.y = y});
    global_tex.draw_texture(t_rainbow_bg, {.y = y});
}

// start() is called while the next screen loads; starting the animations then would put them ahead of
// the script's own frame-driven timing by the load time, cutting the transition short
void Transition::start() {
    start_pending = true;
}

void Transition::update(double current_ms) {
    if (start_pending) {
        start_pending = false;
        call(fn_start, "SongTransition:start");
        dan_start_ms = get_current_ms();
    }
    // dan course with neither loading art nor a rainbow backdrop: no transition at all
    if (dan_color >= 0 && !global_tex.has_texture("loading_dan/night") && !t_rainbow_bg) {
        finished = true;
        return;
    }
    call(fn_update, "SongTransition:update", current_ms, cur_rainbow, cur_fade);
    if (auto s = call_r<sol::table>(fn_state, "SongTransition:state")) {
        cur_rainbow = s->get_or("rainbow_up", 0.0);
        cur_fade = s->get_or("fade", 0.0);
        cur_fade_out = s->get_or("fade_out", 0.0);
        finished = s->get_or("finished", false);
    }
}

bool Transition::is_finished() const {
    return finished;
}

void Transition::draw_song_info() {
    float fade_1 = cur_fade;
    float fade_2 = std::min(0.70, cur_fade);
    float offset = 0;
    if (is_second) {
        fade_1 = cur_fade_out;
        fade_2 = std::min(0.70, cur_fade_out);
        offset = global_tex.get_skin_config(SC::TRANSITION_OFFSET)->y - cur_rainbow;
    }
    global_tex.draw_texture(t_rainbow_text_bg, {.y = static_cast<float>(-cur_rainbow) - offset, .fade = fade_2});

    float x = static_cast<float>(global_tex.screen_width) / 2 - title->width / 2;
    float y = global_tex.get_skin_config(SC::TRANSITION_TITLE)->y - title->height / 2 - cur_rainbow - offset;
    title->draw({.x = x, .y = y, .fade = fade_1});

    x = static_cast<float>(global_tex.screen_width) / 2 - subtitle->width / 2;
    y = global_tex.get_skin_config(SC::TRANSITION_SUBTITLE)->y - subtitle->height / 2 - cur_rainbow - offset;
    subtitle->draw({.x = x, .y = y, .fade = fade_1});
}

void Transition::draw() {
    float total_offset = 0;
    if (is_second) {
        total_offset = global_tex.get_skin_config(SC::TRANSITION_OFFSET)->y;
    }
    // the dan loading art (loading_dan/night, plaque) is optional: without it a dan course shows only the rainbow
    if (dan_color >= 0) {
        if (global_tex.has_texture("loading_dan/night")) {
            draw_dan(total_offset);
        } else if (t_rainbow_bg) {
            draw_dan_rainbow(total_offset);
        }
        return;
    }

    call(fn_draw_bg, "SongTransition:draw_bg", total_offset, cur_rainbow);
    if (!loading_graphic.has_value()) {
        if (fn_draw_info.valid()) {
            call(fn_draw_info, "SongTransition:draw_info", total_offset, cur_rainbow, cur_fade, cur_fade_out);
        } else {
            draw_song_info();
        }
        return;
    }
    if (loading_graphic.has_value()) {
        Rect src = {0, 0, static_cast<float>(loading_graphic.value().width),
                    static_cast<float>(loading_graphic.value().height)};
        Rect dst = {0,
                    global_tex.screen_height +
                        (global_tex.get_skin_config(SC::TRANSITION_OFFSET)->y - global_tex.screen_height) -
                        static_cast<float>(cur_rainbow) - total_offset,
                    static_cast<float>(global_tex.screen_width), static_cast<float>(global_tex.screen_height)};
        draw_texture_pro(loading_graphic.value(), src, dst, {0, 0}, 0, Color::WHITE);
    }

    if (fn_draw_info.valid()) {
        call(fn_draw_info, "SongTransition:draw_info", total_offset, cur_rainbow, cur_fade, cur_fade_out);
    } else {
        draw_song_info();
    }
}
