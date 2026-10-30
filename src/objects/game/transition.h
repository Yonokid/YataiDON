#pragma once

#include "../../libs/file/script.h"
#include "../../libs/gfx/text.h"

class Transition : public LuaScript {
private:
    sol::protected_function fn_update, fn_draw_bg, fn_draw_info, fn_state, fn_start;
    double cur_rainbow = 0, cur_fade = 0, cur_fade_out = 0;
    bool finished = false;
    bool start_pending = false;

    int dan_color = -1;
    double dan_start_ms = 0.0;
    std::unique_ptr<OutlinedText> dan_rank_text;
    void draw_dan(float total_offset);
    void draw_dan_rainbow(float total_offset);

    bool is_second;
    std::unique_ptr<OutlinedText> title;
    std::unique_ptr<OutlinedText> subtitle;
    std::optional<Texture> loading_graphic;
    void draw_song_info();

    TextureObject* t_rainbow_text_bg = nullptr;
    TextureObject* t_loading_dan_night = nullptr;
    TextureObject* t_loading_dan_plaque = nullptr;
    TextureObject* t_rainbow_bg_bottom = nullptr;
    TextureObject* t_rainbow_bg_top = nullptr;
    TextureObject* t_rainbow_bg = nullptr;

public:
    Transition(const std::string& title, const std::string& subtitle, bool is_second);
    ~Transition() override;
    void start();
    void add_loading_graphic(const std::string& path);
    void set_dan(int color, const std::string& rank_name);
    void update(double current_ms);
    void draw();

    bool is_finished() const;
};
