#pragma once
#include "../../libs/config.h"
#include "../../libs/texture.h"
#include "../../libs/animation.h"
#include "../../libs/audio.h"
#include "../../libs/input.h"
#include "../global/nameplate.h"
#include "../global/indicator.h"
#include "box_manager.h"

class EntryPlayer {
private:
    int side;
    BoxManager* box_manager;

    Nameplate* nameplate;
    Indicator* indicator;
    //Chara2D* chara;

    MoveAnimation* drum_move_1;
    MoveAnimation* drum_move_2;
    MoveAnimation* drum_move_3;
    TextureResizeAnimation* cloud_resize;
    TextureResizeAnimation* cloud_resize_loop;
    TextureChangeAnimation* cloud_texture_change;
    FadeAnimation* cloud_fade;

public:
    PlayerNum player_num;
    FadeAnimation* nameplate_fadein;
    EntryPlayer(PlayerNum player_num, int side, BoxManager* box_manager);
    void start_animations();
    void update(double current_time);
    void draw_drum();
    void draw_nameplate_and_indicator(float fade = 1.0f);
    bool is_cloud_animation_finished();
    void handle_input();
};
