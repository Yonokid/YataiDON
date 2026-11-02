#pragma once

#include <memory>
#include <optional>

#include "../../libs/file/scores.h"
#include "../../libs/file/script.h"
#include "../global/chara_3d.h"
#include "../global/indicator.h"
#include "../global/nameplate.h"
#include "box_manager.h"
#include "costume_menu.h"

void save_player_costume(const PlayerData& pd);

class EntryPlayer : public LuaScript {
private:
    int side;
    BoxManager* box_manager;

    std::unique_ptr<Nameplate> nameplate;
    std::unique_ptr<Indicator> indicator;
    std::unique_ptr<Chara3D> chara;
    int chara_index = 0;
    CostumePickStage chara_pick_stage = CostumePickStage::NONE;
    int preset_seq_applied = 0;

    sol::protected_function fn_start_animations;
    sol::protected_function fn_update;
    sol::protected_function fn_draw_drum_back;
    sol::protected_function fn_draw_drum_front;
    sol::protected_function fn_is_cloud_finished;
    sol::protected_function fn_get_nameplate_fade;
    sol::protected_function fn_finish_cloud;
    sol::protected_function fn_get_overlay_pos;
    float nameplate_x = 0.0f, nameplate_y = 0.0f, indicator_x = 0.0f, indicator_y = 0.0f;
    bool loading = false;

public:
    PlayerNum player_num;
    std::optional<CostumeMenu> costume_menu;

    EntryPlayer(PlayerNum player_num, int side, BoxManager* box_manager);
    void start_animations(bool hold_cloud = false);
    void update(double current_time);
    void open_costume_menu();
    void draw_drum();
    void draw_costume_menu();
    void draw_nameplate_and_indicator(float fade = 1.0f);
    bool is_cloud_animation_finished();
    float get_nameplate_fadein();
    void handle_input();
    void refresh_from_player_data();
    void finish_loading(); // data equipped: let the held cloud fade away
};
