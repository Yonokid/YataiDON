#pragma once

#include "../../libs/text.h"

class CoinOverlay {
private:
    std::unique_ptr<OutlinedText> free_play;
public:
    CoinOverlay();
    void update(double current_ms);
    void draw(float x = 0, float y = 0);
};
