#pragma once

#include "../../libs/config.h"
#include "../../libs/texture.h"

class AllNetIcon {
private:
    bool online;
public:
    AllNetIcon();

    void update(double current_ms);

    void draw(float x = 0, float y = 0);
};
