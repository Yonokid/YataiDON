#include "score_history.h"
#include "../../../libs/texture.h"

ScoreHistory::ScoreHistory(const std::array<std::optional<Score>, 5>& scores, double current_ms)
    : last_ms(current_ms)
{
    for (int i = 0; i < 5; i++) {
        if (scores[i].has_value())
            available.push_back({i, scores[i].value()});
    }
    if (!available.empty())
        curr_index = 1 % (int)available.size();
}

void ScoreHistory::update(double current_ms) {
    if (available.empty()) return;
    if (current_ms >= last_ms + 1000.0) {
        last_ms = current_ms;
        curr_index = (curr_index + 1) % (int)available.size();
    }
}

void ScoreHistory::draw() {
    if (available.empty()) return;
    draw_long();
}

void ScoreHistory::draw_long() {
    const auto& [curr_diff, score] = available[curr_index];
    const std::string& score_method = global_data.config->general.score_method;
    float offset_y = tex.skin_config[SC::SCORE_INFO_BG_OFFSET].y;
    float margin_w = tex.skin_config[SC::SCORE_INFO_COUNTER_MARGIN].width;
    float margin_x = tex.skin_config[SC::SCORE_INFO_COUNTER_MARGIN].x;

    tex.draw_texture(LEADERBOARD::BACKGROUND_2, {});
    tex.draw_texture(LEADERBOARD::TITLE, {.index = 1});

    if (score_method == ScoreMethod::SHINUCHI) {
        if (curr_diff == (int)Difficulty::URA)
            tex.draw_texture(LEADERBOARD::SHINUCHI_URA, {.index = 1});
        else
            tex.draw_texture(LEADERBOARD::SHINUCHI, {.index = 1});
        tex.draw_texture(LEADERBOARD::PTS, {.color = ray::WHITE, .index = 1});
    } else {
        tex.draw_texture(LEADERBOARD::NORMAL, {.index = 1});
        tex.draw_texture(LEADERBOARD::PTS, {.color = ray::BLACK, .index = 1});
    }

    tex.draw_texture(LEADERBOARD::DIFFICULTY, {.frame = curr_diff, .index = 1});

    for (int i = 0; i < 4; i++)
        tex.draw_texture(LEADERBOARD::NORMAL, {.y = offset_y + i * offset_y, .index = 1});

    tex.draw_texture(LEADERBOARD::JUDGE_GOOD, {});
    tex.draw_texture(LEADERBOARD::JUDGE_OK, {});
    tex.draw_texture(LEADERBOARD::JUDGE_BAD, {});
    tex.draw_texture(LEADERBOARD::JUDGE_DRUMROLL, {});

    std::array<int, 5> values = {score.score, score.good, score.ok, score.bad, score.drumroll};
    ray::Color score_color = (score_method == ScoreMethod::SHINUCHI) ? ray::WHITE : ray::BLACK;

    for (int j = 0; j < 5; j++) {
        std::string counter = std::to_string(values[j]);
        int len = (int)counter.size();
        if (j == 0) {
            for (int i = 0; i < len; i++) {
                float x = -((len * margin_w) / 2.0f) + (i * margin_w);
                tex.draw_texture(LEADERBOARD::COUNTER, {.color = score_color, .frame = counter[i] - '0', .x = x, .index = 1});
            }
        } else {
            for (int i = 0; i < len; i++) {
                float x = -(float)(len - i) * margin_x;
                float y = (float)j * offset_y;
                tex.draw_texture(LEADERBOARD::JUDGE_NUM, {.frame = counter[i] - '0', .x = x, .y = y});
            }
        }
    }
}

void ScoreHistory::draw_short() {
    if (available.empty()) return;

    const auto& [curr_diff, score] = available[curr_index];
    float offset_y = tex.skin_config[SC::SCORE_INFO_BG_OFFSET].y;
    float margin_w = tex.skin_config[SC::SCORE_INFO_COUNTER_MARGIN].width;

    tex.draw_texture(LEADERBOARD::BACKGROUND, {});
    tex.draw_texture(LEADERBOARD::TITLE, {});

    ray::Color color = ray::BLACK;
    if (curr_diff == (int)Difficulty::URA) {
        tex.draw_texture(LEADERBOARD::NORMAL_URA, {});
        tex.draw_texture(LEADERBOARD::SHINUCHI_URA, {});
        color = ray::WHITE;
        tex.draw_texture(LEADERBOARD::URA, {});
    } else {
        tex.draw_texture(LEADERBOARD::NORMAL, {});
        tex.draw_texture(LEADERBOARD::SHINUCHI, {});
    }

    tex.draw_texture(LEADERBOARD::PTS, {.color = color});
    tex.draw_texture(LEADERBOARD::PTS, {.color = color, .y = offset_y});
    tex.draw_texture(LEADERBOARD::DIFFICULTY, {.frame = curr_diff});

    std::string counter = std::to_string(score.score);
    float total_width = (float)counter.size() * margin_w;
    for (int i = 0; i < (int)counter.size(); i++) {
        float x = -(total_width / 2.0f) + (float)i * margin_w;
        tex.draw_texture(LEADERBOARD::COUNTER, {.color = color, .frame = counter[i] - '0', .x = x});
    }
    for (int i = 0; i < (int)counter.size(); i++) {
        float x = -(total_width / 2.0f) + (float)i * margin_w;
        tex.draw_texture(LEADERBOARD::COUNTER, {.color = ray::WHITE, .frame = counter[i] - '0', .x = x, .y = offset_y});
    }
}
