#pragma once

#include "../../libs/texture.h"
#include "../../libs/audio.h"

class WarningX {
private:
    TextureResizeAnimation* resize;
    FadeAnimation* fade_in;
    FadeAnimation* fade_in_2;
    bool sound_played;

public:
    WarningX();

    void update(double current_ms);

    void draw_bg();

    void draw_fg();

    bool is_finished();
};

class WarningBachiHit {
private:
    TextureResizeAnimation* resize;
    FadeAnimation* fade_in;
    bool sound_played;
public:
    WarningBachiHit();

    void update(double current_ms);

    void draw();

    bool is_finished();
};

class WarningCharacters {
private:
    FadeAnimation* shadow_fade;
    TextureChangeAnimation* chara_0_frame;
    TextureChangeAnimation* chara_1_frame;
    int saved_frame;
public:
    WarningCharacters();

    void update(double current_ms);

    void draw(float fade, float fade_2, float y_pos);

    bool is_finished();
};

class Board {
private:
    MoveAnimation* move_down;
    MoveAnimation* move_up;
    MoveAnimation* move_center;
public:
    float y_pos;
    Board();

    void update(double current_ms);

    void draw();
};

class WarningScreen {
private:
    double start_ms;
    FadeAnimation* fade_in;
    FadeAnimation* fade_out;

    std::unique_ptr<Board> board;
    std::unique_ptr<WarningX> warning_x;
    std::unique_ptr<WarningBachiHit> warning_bachi_hit;
    std::unique_ptr<WarningCharacters> characters;
public:
    WarningScreen(double current_ms);

    void update(double current_ms);

    void draw();

    bool is_finished();
};
