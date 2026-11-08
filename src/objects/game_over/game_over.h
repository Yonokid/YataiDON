#pragma once

#include <optional>

#include "../../libs/file/script.h"

class GameOverSequence : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_finished;

public:
    // overlay: drawn over the screen that ended the credit, with no advertisement first
    explicit GameOverSequence(bool overlay = false);
    void update(double current_ms);
    void draw();
    bool is_finished();
};

// Game over drawn on top of the screen that ends the credit, instead of the GAME_OVER screen's ad.
// The host stays on screen (frozen) underneath.
class GameOverOverlay {
    std::optional<GameOverSequence> sequence;

public:
    void start();
    void reset() { sequence.reset(); }
    bool active() const { return sequence.has_value(); }
    void update(double current_ms);
    bool is_finished();
    void draw();
};
