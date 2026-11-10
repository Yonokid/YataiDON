#include "game_over.h"

#include "../../libs/audio/audio.h"
#include "../../libs/gfx/texture.h"

GameOverSequence::GameOverSequence(bool overlay) {
    if (!load("GameOver", "game_over", overlay)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_draw = lua_object["draw"];
    fn_is_finished = lua_object["is_finished"];
}

void GameOverSequence::update(double current_ms) {
    call(fn_update, "GameOver:update", current_ms);
}

void GameOverSequence::draw() {
    call(fn_draw, "GameOver:draw");
}

bool GameOverSequence::is_finished() {
    return call_r<bool>(fn_is_finished, "GameOver:is_finished").value_or(true);
}

void GameOverOverlay::start() {
    if (sequence.has_value()) {
        return;
    }
    // added to the host's assets; the host's on_screen_end unloads them with its own
    tex.load_folder("game_over", "global");
    audio.load_screen_sounds("game_over");
    // the credit-ending screen's bgm must not play under the curtain (the jingle is the only music here)
    if (audio.has_sound("bgm")) {
        audio.stop_sound("bgm");
    }
    sequence.emplace(true);
}

void GameOverOverlay::update(double current_ms) {
    if (sequence.has_value()) {
        sequence->update(current_ms);
    }
}

bool GameOverOverlay::is_finished() {
    return sequence.has_value() && sequence->is_finished();
}

void GameOverOverlay::draw() {
    if (sequence.has_value()) {
        sequence->draw();
    }
}
