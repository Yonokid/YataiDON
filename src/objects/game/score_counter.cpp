#include "score_counter.h"

ScoreCounter::ScoreCounter(bool is_2p, int score)
    : is_2p(is_2p), score(score) {
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

void ScoreCounter::draw() {
    std::string counter = std::to_string(score);

    float x = 150 * tex.screen_scale;
    float y = (185 * tex.screen_scale) + (is_2p*310*tex.screen_scale);
    float margin = tex.skin_config["score_counter_margin"].x;
    float total_width = counter.length() * margin;
    float start_x = x - total_width;
    for (int i = 0; i < counter.size(); i++) {
        char digit = counter[i];
        tex.draw_texture("lane", "score_number", {.frame=digit - '0', .x=start_x + (i * margin), .y=(float)(y - stretch->attribute), .y2=(float)stretch->attribute});
    }
}
