#pragma once
#include "../../libs/texture.h"
#include "../../libs/audio.h"
#include "../../libs/text.h"
#include "../../libs/global_data.h"

class ModifierSelector {
private:
    static const std::map<std::string, std::string> TEX_MAP;
    static const std::array<std::string, 5> MOD_NAMES;

    PlayerNum player_num;
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

    bool get_bool(int mod_index);
    void set_bool(int mod_index, bool value);

    std::unique_ptr<OutlinedText> make_text(const std::string& str);
    void start_text_animation(int direction);
    void draw_animated_text(const std::unique_ptr<OutlinedText>& text_primary, const std::unique_ptr<OutlinedText>& text_secondary, float x, float y, bool should_animate);

public:
    bool is_finished;
    bool is_confirmed;
    MoveAnimation* move;

    ModifierSelector(PlayerNum player_num);
    void update(double current_ms);
    void confirm();
    void left();
    void right();
    void draw();
};
