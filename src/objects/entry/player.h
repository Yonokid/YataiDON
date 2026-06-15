#pragma once

#include "box_manager.h"
#include "costume_menu.h"
#include "../global/nameplate.h"
#include "../global/indicator.h"
#include "../global/chara_3d.h"
#include "../../libs/script.h"

class EntryPlayer : public LuaScript {
private:
    int side;
    BoxManager* box_manager;

    std::unique_ptr<Nameplate> nameplate;
    std::unique_ptr<Indicator> indicator;
    std::unique_ptr<Chara3D> chara;
    int chara_index = 0;

    sol::protected_function fn_start_animations;
    sol::protected_function fn_update;
    sol::protected_function fn_draw_drum_back;
    sol::protected_function fn_draw_drum_front;
    sol::protected_function fn_is_cloud_finished;
    sol::protected_function fn_get_nameplate_fade;

public:
    PlayerNum player_num;
    std::optional<CostumeMenu> costume_menu;

    EntryPlayer(PlayerNum player_num, int side, BoxManager* box_manager);
    void start_animations();
    void update(double current_time);
    void open_costume_menu();
    void draw_drum();
    void draw_costume_menu();
    void draw_nameplate_and_indicator(float fade = 1.0f);
    bool is_cloud_animation_finished();
    float get_nameplate_fadein();
    void handle_input();
};
