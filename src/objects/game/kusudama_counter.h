#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"

class KusudamaCounter {
private:
    int balloon_total;
    int balloon_count;
    bool is_popped;
    MoveAnimation* move_down;
    MoveAnimation* move_up;
    MoveAnimation* renda_move_up;
    MoveAnimation* renda_move_down;
    FadeAnimation* renda_fade_in;
    FadeAnimation* renda_fade_out;
    TextStretchAnimation* stretch;
    MoveAnimation* breathing;
    MoveAnimation* renda_breathe;
    TextureChangeAnimation* open;
    FadeAnimation* fade_out;
public:
    KusudamaCounter(int total);

    void update_count(int count);

    void update(double current_ms, bool popped);

    void draw();

    bool is_finished() const;
};
