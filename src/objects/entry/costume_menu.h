#pragma once

#include "../../libs/script.h"
#include "../../libs/global_data.h"
#include <memory>
#include <vector>

class CostumeMenu : public LuaScript {
public:
    PlayerNum player_num;
    bool is_2p = false;
    int selected_index = 0;
    bool costume_select_mode = false;
    int costume_icon_index = 0;

    bool confirmed = false;

    CostumeMenu(PlayerNum player_num);
    ~CostumeMenu();
    void update(double current_time_ms);
    void handle_input();
    void draw(float x = 0.0f, float y = 0.0f);
    std::optional<int> get_index();
    std::string get_costume_name() const;

private:
    sol::protected_function fn_update;
    sol::protected_function fn_draw_bg;
    sol::protected_function fn_draw_fg;

    std::vector<ray::Texture2D> costume_icons;
    std::vector<int> costume_ids;
    bool icons_loaded = false;

    void load_costume_icons();

    static constexpr int NUM_ITEMS = 7;
    static constexpr std::array<uint32_t, NUM_ITEMS> ITEMS = {
        COSTUME_SELECT::COSTUME,
        COSTUME_SELECT::DEFAULT,
        COSTUME_SELECT::HEAD_BODY,
        COSTUME_SELECT::PRESET_1,
        COSTUME_SELECT::PRESET_2,
        COSTUME_SELECT::PRESET_3,
        COSTUME_SELECT::RANDOM_ITEM,
    };
};
