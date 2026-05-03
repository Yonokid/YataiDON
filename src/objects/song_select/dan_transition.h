#pragma once

#include "../../libs/animation.h"

class DanTransition {
private:
    MoveAnimation* slide_in;
    bool started;
public:
    DanTransition();
    void start();
    void update(double current_ms);
    void draw();
    bool is_started();
    bool is_finished();
};
