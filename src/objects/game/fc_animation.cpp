#include "fc_animation.h"
#include "../../libs/audio_engine.h"

FCAnimation::FCAnimation(bool is_2p)
    : is_2p(is_2p), draw_clear_full(false), name("in"), frame(0) {

    bachio_fade_in = (FadeAnimation*)tex.get_animation(46);
    bachio_texture_change = (TextureChangeAnimation*)tex.get_animation(47);
    bachio_out = (TextureChangeAnimation*)tex.get_animation(55);
    bachio_move_out = (MoveAnimation*)tex.get_animation(49);

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

    fc_highlight_up = (MoveAnimation*)tex.get_animation(57);
    fc_highlight_up->start();

    fc_highlight_fade_out = (FadeAnimation*)tex.get_animation(58);
    bachio_move_out_2 = (MoveAnimation*)tex.get_animation(59);
    bachio_move_up = (MoveAnimation*)tex.get_animation(60);
    fan_fade_in = (FadeAnimation*)tex.get_animation(61);
    fan_texture_change = (TextureChangeAnimation*)tex.get_animation(62);

    audio->playSound("full_combo", "sound");
}

void FCAnimation::update(double current_ms) {
    bachio_fade_in->update(current_ms);
    bachio_texture_change->update(current_ms);
    bachio_out->update(current_ms);
    bachio_move_out->update(current_ms);
    clear_highlight_fade_in->update(current_ms);
    fc_highlight_up->update(current_ms);
    fc_highlight_fade_out->update(current_ms);
    bachio_move_out_2->update(current_ms);
    bachio_move_up->update(current_ms);
    fan_fade_in->update(current_ms);
    fan_texture_change->update(current_ms);

    if (fc_highlight_up->is_finished && !fc_highlight_fade_out->is_started) {
        fc_highlight_fade_out->start();
        bachio_move_out_2->start();
        bachio_move_up->start();
        fan_fade_in->start();
        fan_texture_change->start();
        audio->playSound("full_combo_voice", "voice");
    }

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

void FCAnimation::draw() {
    if (draw_clear_full) {
        tex.draw_texture("ending_anim", "full_combo_overlay", {
            .y = (float)(-fc_highlight_up->attribute),
            .fade = 0.5f,
            .index = (int)is_2p
        });

        tex.draw_texture("ending_anim", "full_combo", {
            .y = (float)(-fc_highlight_up->attribute),
            .index = (int)is_2p
        });

        tex.draw_texture("ending_anim", "full_combo_highlight", {
            .y = (float)(-fc_highlight_up->attribute),
            .fade = (float)(fc_highlight_fade_out->attribute),
            .index = (int)is_2p
        });

        tex.draw_texture("ending_anim", "fan_l", {
            .frame = (int)fan_texture_change->attribute,
            .fade = (float)(fan_fade_in->attribute),
            .index = (int)is_2p
        });

        tex.draw_texture("ending_anim", "fan_r", {
            .frame = (int)fan_texture_change->attribute,
            .fade = (float)(fan_fade_in->attribute),
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
        .x = (float)((-bachio_move_out->attribute - bachio_move_out_2->attribute) * 1.15f),
        .y = (float)(-bachio_move_up->attribute),
        .fade = (float)(bachio_fade_in->attribute),
        .index = (int)is_2p
    });

    tex.draw_texture("ending_anim", "bachio_r_" + name, {
        .frame = frame,
        .x = (float)((bachio_move_out->attribute + bachio_move_out_2->attribute) * 1.15f),
        .y = (float)(-bachio_move_up->attribute),
        .fade = (float)(bachio_fade_in->attribute),
        .index = (int)is_2p
    });
}
