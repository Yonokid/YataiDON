#pragma once

#include <string>
#include <vector>

class ScoreAnimator {
private:
    std::string target_score;
    std::vector<std::pair<int,int>> current_score_list;
    int digit_index;
public:
    ScoreAnimator(int target);

    std::string next_score();

    bool is_finished;
};
