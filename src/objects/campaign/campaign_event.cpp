#include "campaign_event.h"

CampaignEvent::CampaignEvent(const std::string& campaign_id) {
    if (!load("CampaignEvent", "campaign_event", campaign_id)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_input = lua_object["input"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
    fn_get_reward = lua_object["get_reward"];
    fn_close = lua_object["close"];
}

CampaignEvent::~CampaignEvent() {
    if (fn_close.valid()) {
        call(fn_close, "CampaignEvent:close");
    }
}

void CampaignEvent::update(double current_ms) {
    call(fn_update, "CampaignEvent:update", current_ms);
}
bool CampaignEvent::input_decide() {
    return call_r<bool>(fn_input, "CampaignEvent:input", 3).value_or(false);
}
void CampaignEvent::draw() {
    call(fn_draw, "CampaignEvent:draw");
}
bool CampaignEvent::is_done() {
    return call_r<bool>(fn_is_done, "CampaignEvent:is_done").value_or(true);
}
int CampaignEvent::get_reward() {
    return call_r<int>(fn_get_reward, "CampaignEvent:get_reward").value_or(-1);
}
