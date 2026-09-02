#include "fc_animation.h"
#include "../../libs/texture.h"
#include "../../libs/audio.h"

FCAnimation::FCAnimation(bool is_2p, bool donderful)
    : is_2p(is_2p), draw_clear_full(false), name("in"), frame(0) {

    const bool has_dfc_tex = donderful &&
        tex.has_texture("ending_donderful/full_combo") &&
        tex.has_texture("ending_donderful/full_combo_highlight") &&
        tex.has_texture("ending_donderful/full_combo_overlay");
    const std::string subset = has_dfc_tex ? "ending_donderful/" : "ending_anim/";
    combo_tex           = tex.get_enum(subset + "full_combo");
    combo_highlight_tex = tex.get_enum(subset + "full_combo_highlight");
    combo_overlay_tex   = tex.get_enum(subset + "full_combo_overlay");

    has_panel = has_dfc_tex && tex.has_texture("ending_donderful/background");
    panel_tex = has_panel ? tex.get_enum("ending_donderful/background") : 0;
    panel_fade_in = new FadeAnimation(250, 0.0, false, false, 1.0, 0.0);

    const bool has_dfc_sound = donderful && audio.has_sound("donderful_combo");
    combo_sound = has_dfc_sound ? "donderful_combo" : "full_combo";
    combo_voice = (donderful && audio.has_sound("donderful_combo_voice"))
                  ? "donderful_combo_voice" : "full_combo_voice";

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

    audio.play_sound(combo_sound, VolumePreset::SOUND);
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
        audio.play_sound(combo_voice, VolumePreset::VOICE);
    }

    if (clear_highlight_fade_in->attribute == 1.0f) {
        if (!draw_clear_full && has_panel) panel_fade_in->start();
        draw_clear_full = true;
    }
    panel_fade_in->update(current_ms);

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
        if (has_panel) {
            tex.draw_texture(panel_tex, {
                .fade = (float)(panel_fade_in->attribute),
                .index = (int)is_2p
            });
        }
        tex.draw_texture(ENDING_ANIM::FAN_L, {
            .frame = (int)fan_texture_change->attribute,
            .fade = (float)(fan_fade_in->attribute),
            .index = (int)is_2p
        });

        tex.draw_texture(ENDING_ANIM::FAN_R, {
            .frame = (int)fan_texture_change->attribute,
            .fade = (float)(fan_fade_in->attribute),
            .index = (int)is_2p
        });

        tex.draw_texture(combo_overlay_tex, {
            .y = (float)(-fc_highlight_up->attribute),
            .fade = 0.5f,
            .index = (int)is_2p
        });

        tex.draw_texture(combo_tex, {
            .y = (float)(-fc_highlight_up->attribute),
            .index = (int)is_2p
        });

        tex.draw_texture(combo_highlight_tex, {
            .y = (float)(-fc_highlight_up->attribute),
            .fade = (float)(fc_highlight_fade_out->attribute),
            .index = (int)is_2p
        });
    } else {
        for (int i = 4; i >= 0; i--) {
            tex.draw_texture(ENDING_ANIM::CLEAR_SEPARATED, {
                .frame = i,
                .x = (float)(i * tex.skin_config[SC::CLEAR_ANIMATION_X_SPACING].x),
                .y = (float)(-clear_separate_stretch[i]->attribute),
                .y2 = (float)(clear_separate_stretch[i]->attribute),
                .fade = (float)(clear_separate_fade_in[i]->attribute),
                .index = (int)is_2p
            });
        }
    }

    tex.draw_texture(ENDING_ANIM::CLEAR_HIGHLIGHT, {
        .fade = (float)(clear_highlight_fade_in->attribute),
        .index = (int)is_2p
    });

    tex.draw_texture(tex.get_enum("ending_anim/bachio_l_" + name), {
        .frame = frame,
        .x = (float)((-bachio_move_out->attribute - bachio_move_out_2->attribute) * 1.15f),
        .y = (float)(-bachio_move_up->attribute),
        .fade = (float)(bachio_fade_in->attribute),
        .index = (int)is_2p
    });

    tex.draw_texture(tex.get_enum("ending_anim/bachio_r_" + name), {
        .frame = frame,
        .x = (float)((bachio_move_out->attribute + bachio_move_out_2->attribute) * 1.15f),
        .y = (float)(-bachio_move_up->attribute),
        .fade = (float)(bachio_fade_in->attribute),
        .index = (int)is_2p
    });
}
