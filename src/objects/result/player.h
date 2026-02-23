#pragma once

#include "../../libs/texture.h"
#include "../../libs/audio.h"
#include "result_crown.h"
#include "bottom_characters.h"
#include "../enums.h"
#include "gauge.h"
#include "high_score_indicator.h"
#include "score_animator.h"

class ResultPlayer {
private:
    PlayerNum player_num;
    bool has_2p;
    bool is_2p;

    bool fade_in_finished;
    FadeAnimation* fade_in_bottom;

    std::optional<double> score_delay;
    int update_index = 0;
    std::vector<std::tuple<std::string, int>> update_list;

    std::optional<ResultState> state;

    std::string score = "";
    std::string good = "";
    std::string ok = "";
    std::string bad = "";
    std::string max_combo = "";
    std::string total_drumroll = "";

    BottomCharacters bottom_characters;
    std::optional<ResultGauge> gauge;
    std::optional<ResultCrown> crown;
    std::optional<HighScoreIndicator> high_score_indicator;
    std::optional<ScoreAnimator> score_animator;
    CrownType crown_type;

    void update_score_animation(double current_ms, bool is_skipped);

    void draw_score_info();
    void draw_total_score();
    void draw_modifiers();
public:

    ResultPlayer() = default;
    ResultPlayer(PlayerNum player_num, bool has_2p, bool is_2p);

    void update(double current_ms, bool fade_in_finished, bool is_skipped);

    void draw();
};
