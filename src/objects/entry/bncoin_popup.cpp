#include "bncoin_popup.h"

BanaCoinPopup::BanaCoinPopup(const std::string& kind, int balance, int cost) {
    if (!load("BanaCoinPopup", "bncoin_popup", kind, balance, cost)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_input = lua_object["input"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
    fn_get_result = lua_object["get_result"];
}

void BanaCoinPopup::update(double current_ms) {
    call(fn_update, "BanaCoinPopup:update", current_ms);
}
bool BanaCoinPopup::input(Action action) {
    return call_r<bool>(fn_input, "BanaCoinPopup:input", static_cast<int>(action)).value_or(false);
}
void BanaCoinPopup::draw() {
    call(fn_draw, "BanaCoinPopup:draw");
}
bool BanaCoinPopup::is_done() {
    return call_r<bool>(fn_is_done, "BanaCoinPopup:is_done").value_or(true);
}
bool BanaCoinPopup::paid() {
    return call_r<std::string>(fn_get_result, "BanaCoinPopup:get_result").value_or("") == "pay";
}
