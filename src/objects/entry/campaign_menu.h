#pragma once

#include <string>
#include <vector>

#include "../../libs/file/script.h"

class CampaignMenu : public LuaScript {
public:
    enum Action {
        NONE = 0,
        LEFT = 1,
        RIGHT = 2,
        DECIDE = 3
    };

    explicit CampaignMenu(const std::vector<std::string>& campaign_ids);
    bool unavailable() const { return !fn_input.valid(); }
    void update(double current_ms);
    bool input(Action action); // false: ignored (still opening or turning)
    void draw();
    bool is_done();
    std::string get_choice(); // "skip": none

private:
    sol::protected_function fn_update;
    sol::protected_function fn_input;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;
    sol::protected_function fn_get_choice;
};
