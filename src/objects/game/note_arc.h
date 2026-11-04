#pragma once

#include "../../libs/file/script.h"
#include "../../libs/global_data.h"
#include "../../libs/parsers/tja.h"

// Timing/lifetime only; the curve and drawing live in the skin's note_arc.lua.
class NoteArc : public LuaScript {
private:
    bool is_balloon;
    int arc_duration{31};
    float current_progress{0};
    double elapsed_ms{0};
    bool note_finished_handled{false};
    double start_ms;
    int firework_frames = 0; // Lua-reported length of the trailing fireworks

    sol::protected_function fn_draw;

public:
    NoteType note_type;
    bool is_big;

    NoteArc(NoteType note_type, double current_ms, PlayerNum player_num, bool big, bool is_balloon, float start_x = 0,
            float start_y = 0, bool rainbow_arc = false, bool synchro = false);

    void update(double current_ms);

    void draw(float y);

    bool is_finished() const;

    bool is_note_finished() const;
    bool consume_note_finished();
};
