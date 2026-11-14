#include "skill_picker.h"

#include <algorithm>

#include "../../libs/audio/audio.h"
#include "../../libs/input/input.h"
#include "../game/battle_match.h"

SkillPicker::SkillPicker(int moves, int player) : items(battle_slot_items(moves)), moves(moves) {
    auto& slots = global_data.battle_skills;
    if (slots.size() != BATTLE_SKILL_SLOTS) {
        slots = {BATTLE_MOVE_UNLOCKS[0], BATTLE_MOVE_NONE, BATTLE_MOVE_NONE};
    }
    for (int& move : slots) {
        if (!std::ranges::contains(items, move)) {
            move = BATTLE_MOVE_NONE;
        }
    }
    if (!script_manager.lua) {
        return;
    }
    if (!load("BattleSkillPicker", "skill_picker", script_manager.lua->create_table_with("player", player))) {
        return;
    }
    fn_draw = lua_object["draw"];
    fn_open = lua_object["open"];
    fn_move = lua_object["move"];
    fn_decide = lua_object["decide"];
}

// the panel's counter: the move's place among the unlocked ones, 0 on random and none
int SkillPicker::number(int index) const {
    return index < moves ? index + 1 : 0;
}

void SkillPicker::open(double ms) {
    if (!lua_object.valid()) {
        return;
    }
    cursor.clear();
    for (const int move : global_data.battle_skills) {
        cursor.push_back(static_cast<int>(std::ranges::find(items, move) - items.begin()));
    }
    slot = 0;
    choosing = true;
    call(fn_open, "BattleSkillPicker:open", ms, sol::as_table(global_data.battle_skills), number(cursor[0]), moves);
}

bool SkillPicker::handle_input(double ms) {
    if (!choosing) {
        return false;
    }
    const PlayerNum pn = global_data.player_num;
    const int dir = is_l_kat_pressed(pn) ? -1 : is_r_kat_pressed(pn) ? 1 : 0;
    const int count = static_cast<int>(items.size());
    if (dir != 0) {
        audio.play_player_sound("kat", pn, VolumePreset::HITSOUND);
        cursor[slot] = (cursor[slot] + dir + count) % count;
        call(fn_move, "BattleSkillPicker:move", ms, slot, items[cursor[slot]], number(cursor[slot]), dir);
    } else if (is_l_don_pressed(pn) || is_r_don_pressed(pn)) {
        audio.play_player_sound("don", pn, VolumePreset::HITSOUND);
        global_data.battle_skills[slot] = items[cursor[slot]];
        call(fn_decide, "BattleSkillPicker:decide", ms, slot,
             slot + 1 < BATTLE_SKILL_SLOTS ? number(cursor[slot + 1]) : 0);
        choosing = ++slot < BATTLE_SKILL_SLOTS;
    }
    return true;
}

void SkillPicker::draw(double ms) {
    call(fn_draw, "BattleSkillPicker:draw", ms);
}
