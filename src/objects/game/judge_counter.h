#pragma once

#include "../../libs/ray.h"

class JudgeCounter {
private:
    int good;
    int ok;
    int bad;
    int drumrolls;
    ray::Color orange;
    ray::Color white;

    void draw_counter(float counter, float x, float y, float margin, ray::Color color);

public:
    JudgeCounter();

    void update(int good, int ok, int bad, int drumrolls);
    void draw();
};
