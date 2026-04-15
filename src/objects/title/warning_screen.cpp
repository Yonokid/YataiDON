#include "warning_screen.h"

WarningX::WarningX() {
    resize = (TextureResizeAnimation*)tex.get_animation(0);
    resize->start();
    fade_in = (FadeAnimation*)tex.get_animation(1);
    fade_in->start();
    fade_in_2 = (FadeAnimation*)tex.get_animation(2);
    fade_in_2->start();
    sound_played = false;
}

void WarningX::update(double current_ms) {
    resize->update(current_ms);
    fade_in->update(current_ms);
    fade_in_2->update(current_ms);

    if (resize->attribute > 1 && !sound_played) {
        audio->play_sound("error", "attract_mode");
        sound_played = true;
    }
}

void WarningX::draw_bg() {
    tex.draw_texture(WARNING::X_LIGHTRED, {.fade=fade_in_2->attribute});
}

void WarningX::draw_fg() {
    tex.draw_texture(WARNING::X_RED, {.scale=(float)resize->attribute, .center=true, .fade=fade_in->attribute});
}


WarningBachiHit::WarningBachiHit() {
    resize = (TextureResizeAnimation*)tex.get_animation(3);
    fade_in = (FadeAnimation*)tex.get_animation(4);

    sound_played = false;
}

void WarningBachiHit::update(double current_ms) {
    if (!sound_played) {
        audio->play_sound("bachi_hit", "attract_mode");
        sound_played = true;
        fade_in->start();
        resize->start();
    }
    resize->update(current_ms);
    fade_in->update(current_ms);
}

void WarningBachiHit::draw() {
    tex.draw_texture(WARNING::BACHI_HIT, {.scale=(float)resize->attribute, .center=true, .fade=fade_in->attribute});
    if (resize->attribute > 0 && sound_played) {
        tex.draw_texture(WARNING::BACHI);
    }
}

WarningCharacters::WarningCharacters() {
    shadow_fade = (FadeAnimation*)tex.get_animation(5);
    chara_0_frame = (TextureChangeAnimation*)tex.get_animation(7);
    chara_1_frame = (TextureChangeAnimation*)tex.get_animation(6);
    chara_0_frame->start();
    chara_1_frame->start();
    saved_frame = 0;
}

void WarningCharacters::update(double current_ms) {
    shadow_fade->update(current_ms);
    chara_1_frame->update(current_ms);
    chara_0_frame->update(current_ms);

    if (chara_1_frame->attribute != saved_frame) {
        saved_frame = chara_1_frame->attribute;
        if (!shadow_fade->is_started) {
            shadow_fade->start();
        }
    }
}

void WarningCharacters::draw(float fade, float fade_2, float y_pos) {
    tex.draw_texture(WARNING::CHARA_0_SHADOW, {.y=y_pos, .fade=fade_2});
    tex.draw_texture(WARNING::CHARA_0, {.frame=(int)chara_0_frame->attribute, .y=y_pos, .fade=fade});

    tex.draw_texture(WARNING::CHARA_1_SHADOW, {.y=y_pos, .fade=fade_2});
    if (-1 < chara_1_frame->attribute-1 && chara_1_frame->attribute-1 < 7) {
        tex.draw_texture(WARNING::CHARA_1, {.frame=(int)chara_1_frame->attribute-1, .y=y_pos, .fade=shadow_fade->attribute});
    }
    tex.draw_texture(WARNING::CHARA_1, {.frame=(int)chara_1_frame->attribute, .y=y_pos, .fade=fade});
}

bool WarningCharacters::is_finished() {
    return chara_1_frame->is_finished;
}

Board::Board() {
    move_down = (MoveAnimation*)tex.get_animation(10);
    move_down->start();
    move_up = (MoveAnimation*)tex.get_animation(11);
    move_up->start();
    move_center = (MoveAnimation*)tex.get_animation(12);
    move_center->start();
    y_pos = 0;
}

void Board::update(double current_ms) {
    move_down->update(current_ms);
    move_up->update(current_ms);
    move_center->update(current_ms);
    if (move_up->is_finished) {
        y_pos = move_center->attribute;
    } else if (move_down->is_finished) {
        y_pos = move_up->attribute;
    } else {
        y_pos = move_down->attribute;
    }
}

void Board::draw() {
    tex.draw_texture(WARNING::WARNING_BOX, {.y=y_pos});
}

WarningScreen::WarningScreen(double current_ms) : start_ms(current_ms) {
    fade_in = (FadeAnimation*)tex.get_animation(8);
    fade_out = (FadeAnimation*)tex.get_animation(9);
    fade_in->start();

    board = new Board();
    warning_x = new WarningX();
    warning_bachi_hit = new WarningBachiHit();
    characters = new WarningCharacters();
}

void WarningScreen::update(double current_ms) {
    board->update(current_ms);
    fade_in->update(current_ms);
    fade_out->update(current_ms);
    double delay = 566.67;
    double elapsed_time = current_ms - start_ms;
    warning_x->update(current_ms);
    characters->update(current_ms);

    if (characters->is_finished()) {
        warning_bachi_hit->update(current_ms);
    } else {
        fade_out->start();
        if (delay <= elapsed_time && !audio->is_sound_playing("bachi_swipe")) {
            audio->play_sound("warning_voiceover", "attract_mode");
            audio->play_sound("bachi_swipe", "attract_mode");
        }
    }
}

bool WarningScreen::is_finished() {
    return fade_out->is_finished;
}

void WarningScreen::draw() {
    board->draw();
    warning_x->draw_bg();
    characters->draw(fade_in->attribute, std::min(fade_in->attribute, 0.75), board->y_pos);
    warning_x->draw_fg();
    warning_bachi_hit->draw();

    tex.draw_texture(MOVIE::BACKGROUND, {.fade=fade_out->attribute});
}
