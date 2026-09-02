#pragma once

#include "../../libs/global_data.h"
#include "../../libs/scores.h"
#include "../../libs/text.h"

class ModifierSelector {
private:
    static const std::map<std::string, std::string> TEX_MAP;
    static const std::array<std::string, 5> BASE_MOD_NAMES;

    std::vector<std::string> mod_names;

    PlayerNum player_num;
    PlayerData* player;
    int current_mod_index;
    std::string language;
    int direction;

    std::vector<Modifiers> mods;

    FadeAnimation* blue_arrow_fade;
    MoveAnimation* blue_arrow_move;
    MoveAnimation* move_sideways;
    FadeAnimation* fade_sideways;

    std::vector<std::unique_ptr<OutlinedText>> text_name;
    std::unique_ptr<OutlinedText> text_true;
    std::unique_ptr<OutlinedText> text_false;
    std::unique_ptr<OutlinedText> text_speed;
    std::unique_ptr<OutlinedText> text_kimagure;
    std::unique_ptr<OutlinedText> text_detarame;

    std::unique_ptr<OutlinedText> text_true_2;
    std::unique_ptr<OutlinedText> text_false_2;
    std::unique_ptr<OutlinedText> text_speed_2;
    std::unique_ptr<OutlinedText> text_kimagure_2;
    std::unique_ptr<OutlinedText> text_detarame_2;

    std::vector<std::string> neiro_names;
    int neiro_index = 0;
    std::string neiro_preview;
    std::unique_ptr<OutlinedText> text_neiro;
    std::unique_ptr<OutlinedText> text_neiro_2;

    bool has_row(const std::string& name) const {
        return std::find(mod_names.begin(), mod_names.end(), name) != mod_names.end();
    }
    bool has_neiro_row() const { return has_row("neiro"); }
    void load_neiro_names();
    void step_neiro(int dir);

    std::string row_label(const std::string& name) const;
    bool get_bool(int mod_index);
    void set_bool(int mod_index, bool value);

    std::unique_ptr<OutlinedText> make_text(const std::string& str);
    void start_text_animation(int direction);
    void draw_animated_text(const std::unique_ptr<OutlinedText>& text_primary, const std::unique_ptr<OutlinedText>& text_secondary, float x, float y, bool should_animate);

public:
    struct ModRow {
        std::string name;
        std::string value;
        std::string label;
        int  state;
        bool changed;
        bool enabled;
        bool greyed;
    };
    bool row_greyed(const std::string& name) const;
    std::vector<ModRow> lua_rows();
    int  lua_index() const { return current_mod_index; }
    int  lua_row_count() const { return (int)mod_names.size(); }
    int   lua_change_dir() const { return direction; }
    float lua_change_fade() const { return (float)fade_sideways->attribute; }
    bool  lua_change_active() const { return !move_sideways->is_finished; }

    bool is_finished;
    bool is_confirmed;
    MoveAnimation* move;
    MoveAnimation* move_out = nullptr;

    ModifierSelector(PlayerNum player_num, PlayerData* player);
    void update(double current_ms);
    void confirm();
    void left();
    void right();
    void draw();
};
