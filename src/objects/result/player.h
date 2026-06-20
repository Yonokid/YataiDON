#pragma once

#include "../global/nameplate.h"
#include "../global/chara_3d.h"
#include "../enums.h"
#include "result_crown.h"
#include "score_animator.h"
#include "../../libs/script.h"

class ResultPlayer : public LuaScript {
    sol::protected_function fn_update, fn_draw, fn_draw_gauge;
    PlayerNum player_num;
    bool has_2p = false;
    bool is_2p  = false;
    Nameplate nameplate;
    std::unique_ptr<Chara3D> chara;
    std::optional<double> score_delay;
    int update_index = 0;
    std::vector<std::tuple<std::string, int>> update_list;
    std::string score = "", good = "", ok = "", bad = "", max_combo = "", total_drumroll = "";
    std::optional<ScoreAnimator> score_animator;
    bool high_score_sound_played = false;
    void update_score_animation(double current_ms, bool is_skipped);
public:
    ResultPlayer() = default;
    ResultPlayer(PlayerNum player_num, bool has_2p, bool is_2p);
    void update(double current_ms, bool fade_in_finished, bool is_skipped);
    void draw();
};
