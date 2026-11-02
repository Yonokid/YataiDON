#include "campaign_menu.h"

CampaignMenu::CampaignMenu(const std::vector<std::string>& campaign_ids) {
    if (!load("CampaignMenu", "campaign_menu", sol::as_table(campaign_ids))) {
        return;
    }
    fn_update = lua_object["update"];
    fn_input = lua_object["input"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
    fn_get_choice = lua_object["get_choice"];
}

void CampaignMenu::update(double current_ms) {
    call(fn_update, "CampaignMenu:update", current_ms);
}
bool CampaignMenu::input(Action action) {
    return call_r<bool>(fn_input, "CampaignMenu:input", static_cast<int>(action)).value_or(false);
}
void CampaignMenu::draw() {
    call(fn_draw, "CampaignMenu:draw");
}
bool CampaignMenu::is_done() {
    return call_r<bool>(fn_is_done, "CampaignMenu:is_done").value_or(true);
}
std::string CampaignMenu::get_choice() {
    return call_r<std::string>(fn_get_choice, "CampaignMenu:get_choice").value_or("skip");
}
