#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../../libs/file/scores.h"
#include "../../libs/gfx/text.h"
#include "../../libs/global_data.h"
#include "../../libs/parsers/tja.h"

// the battle select puts its skill picker on the neiro slot, so the neiro moves into the option panel
inline bool neiro_in_option_panel() {
    return tex.options[SCO::OPTION_NEIRO_ROW] || global_data.current_screen == "BATTLE_SELECT";
}

class ModifierSelector {
private:
    static const std::array<std::string, 5> BASE_MOD_NAMES;

    std::vector<std::string> mod_names;

    PlayerNum player_num;
    PlayerData* player;
    int current_mod_index;
    std::string language;
    int direction;

    FadeAnimation* blue_arrow_fade;
    MoveAnimation* blue_arrow_move;
    MoveAnimation* move_sideways;
    FadeAnimation* fade_sideways;

    std::vector<std::string> neiro_names;
    int neiro_index = 0;
    std::string neiro_preview;
    int change_seq = 0;
    int instance_id = 0;

    bool has_row(const std::string& name) const { return std::ranges::contains(mod_names, name); }
    bool has_neiro_row() const { return has_row("neiro"); }
    void load_neiro_names();
    void step_neiro(int dir);
    void step_offset(int dir);
    bool offset_dirty = false;

    std::string row_label(const std::string& name) const;
    bool get_bool(int mod_index);
    void set_bool(int mod_index, bool value);

    void start_text_animation(int direction);

public:
    struct ModRow {
        std::string name;
        std::string value;
        std::string label;
        int state;
        bool changed;
        bool enabled;
        bool greyed;
    };
    bool row_greyed(const std::string& name) const;
    std::vector<ModRow> lua_rows();
    int lua_index() const { return current_mod_index; }
    int lua_row_count() const { return static_cast<int>(mod_names.size()); }
    int lua_change_dir() const { return direction; }
    float lua_change_fade() const { return fade_sideways->attribute; }
    bool lua_change_active() const { return !move_sideways->is_finished; }
    int lua_change_seq() const { return change_seq; }
    double lua_change_move() const { return move_sideways->attribute; }
    double lua_arrow_fade() const { return blue_arrow_fade->attribute; }
    double lua_arrow_move() const { return blue_arrow_move->attribute; }
    int lua_id() const { return instance_id; }
    PlayerNum lua_player() const { return player_num; }

    bool is_finished;
    bool is_confirmed;
    MoveAnimation* move;
    MoveAnimation* move_out = nullptr;

    ModifierSelector(PlayerNum player_num, PlayerData* player);
    ~ModifierSelector();
    ModifierSelector(const ModifierSelector&) = delete;
    ModifierSelector& operator=(const ModifierSelector&) = delete;
    ModifierSelector(ModifierSelector&&) = default;
    ModifierSelector& operator=(ModifierSelector&&) = default;
    void update(double current_ms);
    void confirm();
    void left();
    void right();
};
