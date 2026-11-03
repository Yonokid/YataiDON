#pragma once

#include <string>
#include <vector>

#include "../../libs/file/script.h"

class CampaignEvent : public LuaScript {
public:
    CampaignEvent(const std::string& campaign_id, const std::vector<int>& seats);
    ~CampaignEvent() override;
    CampaignEvent(const CampaignEvent&) = delete;
    CampaignEvent& operator=(const CampaignEvent&) = delete;

    bool unavailable() const { return !fn_input.valid(); }
    void update(double current_ms);
    enum Action {
        LEFT = 1,
        RIGHT = 2,
        DECIDE = 3
    };
    bool input(Action action, int player);
    void draw();
    bool is_done();
    int get_reward();
    int get_team(int player);

private:
    sol::protected_function fn_update;
    sol::protected_function fn_input;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;
    sol::protected_function fn_get_reward;
    sol::protected_function fn_close;
    sol::protected_function fn_get_team;
};
