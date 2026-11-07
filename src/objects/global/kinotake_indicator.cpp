#include "kinotake_indicator.h"

#include <algorithm>

#include "../../libs/file/scores.h"
#include "../../libs/network/network.h"

KinotakeIndicator::KinotakeIndicator(const std::string& mode) {
    if (!load("KinotakeIndicator", "kinotake_indicator", mode)) {
        return;
    }
    fn_set = lua_object["set"];
    fn_update = lua_object["update"];
    fn_draw = lua_object["draw"];
}

void KinotakeIndicator::set(int leader, int assist) {
    call(fn_set, "KinotakeIndicator:set", leader, assist);
}
bool KinotakeIndicator::refresh() {
    if (!std::ranges::contains(network.active_campaigns(), "kinotake")) {
        return false;
    }
    int assist = -1;
    for (const PlayerNum seat : {PlayerNum::P1, PlayerNum::P2}) {
        const std::string id = get_player_id(seat);
        const PlayerData* pd = id == scores_manager.player_1   ? &scores_manager.player_1_data
                               : id == scores_manager.player_2 ? &scores_manager.player_2_data
                                                               : nullptr;
        if (pd && pd->chara_is_costume) {
            const int team = kinotake_costume_team(pd->chara_cos_index);
            assist = team == 0 ? 0 : team == 1 ? 4 : -1; // kinoko x2, takenoko x2
        }
        if (assist >= 0) {
            break;
        }
    }
    set(network.kinotake_leader(), assist);
    return true;
}

void KinotakeIndicator::update(double current_ms) {
    call(fn_update, "KinotakeIndicator:update", current_ms);
}
void KinotakeIndicator::draw(float fade) {
    call(fn_draw, "KinotakeIndicator:draw", fade);
}
