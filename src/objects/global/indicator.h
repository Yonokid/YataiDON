#pragma once

#include "../../libs/text.h"

class Indicator {
public:
    enum class State {
        SKIP = 0,
        SIDE = 1,
        SELECT = 2,
        WAIT = 3
    };
private:
    State state;
    FadeAnimation* don_fade;
    MoveAnimation* blue_arrow_move;
    FadeAnimation* blue_arrow_fade;
    std::unique_ptr<OutlinedText> select_text;
public:

    Indicator(State state);
    void update(double current_ms);
    void draw(float x, float y, float fade = 1.0f);
};
