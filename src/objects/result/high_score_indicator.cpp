#include "high_score_indicator.h"

HighScoreIndicator::HighScoreIndicator(int old_score, int new_score, bool is_2p)
: is_2p(is_2p) {
    score_diff = new_score - old_score;
    move = (MoveAnimation*)tex.get_animation(18);
    fade = (FadeAnimation*)tex.get_animation(19);
    move->start();
    fade->start();
}

void HighScoreIndicator::update(double current_ms) {
    move->update(current_ms);
    fade->update(current_ms);
}

void HighScoreIndicator::draw() {
    tex.draw_texture(SCORE::HIGH_SCORE, {.y=(float)move->attribute, .fade=fade->attribute, .index=is_2p});
    std::string score_str = std::to_string(score_diff);
    std::string reversed_score = score_str;
    std::reverse(reversed_score.begin(), reversed_score.end());
    for (int i = 0; i < reversed_score.length(); ++i) {
        int digit = reversed_score[i] - '0';
        tex.draw_texture(SCORE::HIGH_SCORE_NUM, {.frame=digit, .x=-(i*tex.skin_config[SC::HIGH_SCORE_INDICATOR_MARGIN].x), .y=(float)move->attribute, .fade=fade->attribute, .index=is_2p});
    }
}
