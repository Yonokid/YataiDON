#pragma once

#include <string>

#include "../../libs/file/script.h"

class CampaignEvent : public LuaScript {
public:
    explicit CampaignEvent(const std::string& campaign_id);
    ~CampaignEvent() override;
    CampaignEvent(const CampaignEvent&) = delete;
    CampaignEvent& operator=(const CampaignEvent&) = delete;

    bool unavailable() const { return !fn_input.valid(); }
    void update(double current_ms);
    bool input_decide();
    void draw();
    bool is_done();
    int get_reward();

private:
    sol::protected_function fn_update;
    sol::protected_function fn_input;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;
    sol::protected_function fn_get_reward;
    sol::protected_function fn_close;
};
