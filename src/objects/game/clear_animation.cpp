#include "clear_animation.h"

ClearAnimation::ClearAnimation(bool is_2p)
    : is_2p(is_2p), draw_clear_full(false), name("in"), frame(0) {

    bachio_fade_in = (FadeAnimation*)tex.get_animation(46);
    bachio_texture_change = (TextureChangeAnimation*)tex.get_animation(47);
    bachio_out = (TextureChangeAnimation*)tex.get_animation(55);
    bachio_move_out = (MoveAnimation*)tex.get_animation(66);

    bachio_fade_in->start();
    bachio_texture_change->start();
    bachio_out->start();
    bachio_move_out->start();

    for (int i = 0; i < 5; i++) {
        FadeAnimation* fade = new FadeAnimation(100, 1.0f, false, false, 0.0f, i * 50);
        fade->start();
        clear_separate_fade_in.push_back(fade);

        TextStretchAnimation* stretch = new TextStretchAnimation(200, i * 50);
        stretch->start();
        clear_separate_stretch.push_back(stretch);
    }

    clear_highlight_fade_in = (FadeAnimation*)tex.get_animation(56);
    clear_highlight_fade_in->start();

    audio->play_sound("clear", "sound");
}

void ClearAnimation::update(double current_ms) {
    bachio_fade_in->update(current_ms);
    bachio_texture_change->update(current_ms);
    bachio_out->update(current_ms);
    bachio_move_out->update(current_ms);
    clear_highlight_fade_in->update(current_ms);

    if (clear_highlight_fade_in->attribute == 1.0f) {
        draw_clear_full = true;
    }

    for (auto fade : clear_separate_fade_in) {
        fade->update(current_ms);
    }
    for (auto stretch : clear_separate_stretch) {
        stretch->update(current_ms);
    }

    if (bachio_texture_change->is_finished) {
        name = "out";
        frame = (int)bachio_out->attribute;
    } else {
        frame = (int)bachio_texture_change->attribute;
    }
}

void ClearAnimation::draw() {
    if (draw_clear_full) {
        tex.draw_texture("ending_anim", "clear", {
            .index = (int)is_2p
        });
    } else {
        for (int i = 4; i >= 0; i--) {
            tex.draw_texture("ending_anim", "clear_separated", {
                .frame = i,
                .x = (float)(i * 60 * tex.screen_scale),
                .y = (float)(-clear_separate_stretch[i]->attribute),
                .y2 = (float)(clear_separate_stretch[i]->attribute),
                .fade = (float)(clear_separate_fade_in[i]->attribute),
                .index = (int)is_2p
            });
        }
    }

    tex.draw_texture("ending_anim", "clear_highlight", {
        .fade = (float)(clear_highlight_fade_in->attribute),
        .index = (int)is_2p
    });

    tex.draw_texture("ending_anim", "bachio_l_" + name, {
        .frame = frame,
        .x = (float)(-bachio_move_out->attribute),
        .fade = (float)(bachio_fade_in->attribute),
        .index = (int)is_2p
    });

    tex.draw_texture("ending_anim", "bachio_r_" + name, {
        .frame = frame,
        .x = (float)(bachio_move_out->attribute),
        .fade = (float)(bachio_fade_in->attribute),
        .index = (int)is_2p
    });
}
