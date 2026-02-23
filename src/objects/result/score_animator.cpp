#include "score_animator.h"

ScoreAnimator::ScoreAnimator(int target) {
    target_score = std::to_string(target);
    current_score_list = std::vector<std::pair<int,int>>(target_score.size(), {0, 0});
    digit_index = target_score.size() - 1;
    is_finished = false;
}

std::string ScoreAnimator::next_score() {
    if (digit_index == -1) {
        is_finished = true;
        std::string result;
        for (auto& item : current_score_list)
            result += std::to_string(item.first);
        return std::to_string(std::stoi(result));
    }

    auto& [curr_digit, counter] = current_score_list[digit_index];

    if (counter < 9) {
        counter++;
        curr_digit = (curr_digit + 1) % 10;
    } else {
        curr_digit = target_score[digit_index] - '0';
        digit_index--;
    }

    std::string ret_val;
    for (auto& item : current_score_list)
        ret_val += std::to_string(item.first);

    if (std::stoi(ret_val) == 0) {
        int active_digits = target_score.size() - digit_index;
        if (!(active_digits > (int)target_score.size()))
            return std::string(target_score.size() - digit_index, '0');
        return "0";
    }

    return std::to_string(std::stoi(ret_val));
}
