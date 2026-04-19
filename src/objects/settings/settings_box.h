#pragma once
#include "../../libs/config.h"
#include "../../libs/texture.h"
#include "../../libs/animation.h"
#include "../../libs/text.h"
#include "option_box.h"
#include <rapidjson/document.h>
#include <optional>
#include <vector>
#include <string>

class SettingsBox {
private:
    std::unique_ptr<OutlinedText>   label;
    float           x;
    float           y;
    float           start_position;
    float           target_position;
    int             direction;
    BaseAnimation*  move_anim;
    FadeAnimation*  blue_arrow_fade;
    MoveAnimation*  blue_arrow_move;
    bool            is_selected;
    int             option_index;
    std::vector<std::unique_ptr<BaseOptionBox>> options;

    void draw_text() const;

    static std::unique_ptr<BaseOptionBox> make_option_box(const rapidjson::Value& opt);

public:
    std::string box_name;
    bool        in_box;

    SettingsBox(const std::string& name, const std::string& label_text,
                const rapidjson::Value& options_json);
    ~SettingsBox();

    void set_y(float new_y);

    // Returns false when the box should become un-selected (exit to outer navigation)
    bool move_left();
    void move_right();

    bool move_option_left();
    void move_option_right();

    void select_option();

    void select();

    // Returns non-null if an option requested a screen transition
    std::optional<Screens> pending_screen_change() const;

    void update(double current_time_ms, bool selected);
    void draw();
};
