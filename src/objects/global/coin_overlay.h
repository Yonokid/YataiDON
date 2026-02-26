#pragma once
#include "../../libs/config.h"
#include "../../libs/texture.h"
#include "../../libs/text.h"

class CoinOverlay {
private:
    OutlinedText* free_play;
public:
    CoinOverlay();
    void update(double current_ms);
    void draw(float x = 0, float y = 0);
};
