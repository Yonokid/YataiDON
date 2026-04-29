#include "score_counter.h"

ScoreCounter::ScoreCounter(int score, bool is_2p) : score(score), is_2p(is_2p) {
    stretch = (TextStretchAnimation*)tex.get_animation(4, true);
}

void ScoreCounter::update_count(int score) {
    if (score != this->score) {
        this->score = score;
        stretch->start();
    }
}

void ScoreCounter::update(double current_ms, int score) {
    update_count(score);
    if (score > 0) {
        stretch->update(current_ms);
    }
}

void ScoreCounter::draw(float y) {
    float p2_offset = is_2p ? 130 * tex.screen_scale : 0;
    if (is_2p) {
        tex.draw_texture(LANE::LANE_SCORE_COVER, {.mirror="vertical", .y=y + p2_offset});
    } else {
        tex.draw_texture(LANE::LANE_SCORE_COVER, {.y=y});
    }

    std::string counter = std::to_string(score);

    float x = 150 * tex.screen_scale;
    float y_pos = y + (185 * tex.screen_scale) + p2_offset;
    float margin = tex.skin_config[SC::SCORE_COUNTER_MARGIN].x;
    float total_width = counter.length() * margin;
    float start_x = x - total_width;
    for (int i = 0; i < counter.size(); i++) {
        char digit = counter[i];
        tex.draw_texture(LANE::SCORE_NUMBER, {.frame=digit - '0', .x=start_x + (i * margin), .y=(float)(y_pos - stretch->attribute), .y2=(float)stretch->attribute});
    }
}
