#pragma once
#include "../../libs/texture.h"
#include "../../libs/audio.h"
#include "../../enums.h"

class ScoreHistory {
public:
    ScoreHistory(const std::map<int, ScoreRow*>& scores, double current_ms);

    void update(double current_ms);
    void draw();

private:
    std::map<int, ScoreRow*> scores;
    std::vector<int> difficulty_keys;
    int curr_difficulty_index;
    int curr_difficulty;
    int curr_score;
    int curr_score_su;
    double last_ms;
    bool is_long;

    void draw_long();
};
