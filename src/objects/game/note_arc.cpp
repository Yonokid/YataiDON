#include "note_arc.h"

NoteArc::NoteArc(NoteType note_type, double current_ms, PlayerNum player_num, bool big, bool is_balloon, float start_x,
                 float start_y, bool rainbow_arc, bool synchro)
    : is_balloon(is_balloon), start_ms(current_ms), note_type(note_type), is_big(big) {
    NoteType texture_note_type = note_type;
    if (big) {
        if (note_type == NoteType::DON) {
            texture_note_type = NoteType::DON_L;
        } else if (note_type == NoteType::KAT) {
            texture_note_type = NoteType::KAT_L;
        }
    }
    if (note_type == NoteType::BALLOON_HEAD || is_balloon) {
        texture_note_type = NoteType::DON_L;
    }

    if (load("NoteArc", "note_arc", static_cast<int>(texture_note_type), big, is_balloon, static_cast<int>(player_num),
             start_x, start_y, rainbow_arc, synchro)) {
        fn_draw = lua_object["draw"];
        firework_frames = lua_object["firework_frames"].get_or(0);
    }
}

void NoteArc::update(double current_ms) {
    // elapsed_ms keeps running past the arc so trailing fireworks can finish
    elapsed_ms = std::max(0.0, current_ms - start_ms);
    current_progress = std::min(1.0, elapsed_ms / (arc_duration * 16.67));
}

void NoteArc::draw(float y) {
    call(fn_draw, "NoteArc:draw", y, elapsed_ms, arc_duration * 16.67);
}

bool NoteArc::is_finished() const {
    double end_ms = arc_duration * 16.67;
    if (is_balloon) {
        end_ms *= 1.5;
    }
    end_ms += firework_frames * 16.67;
    return elapsed_ms >= end_ms;
}

bool NoteArc::is_note_finished() const {
    return current_progress >= 1.0;
}

bool NoteArc::consume_note_finished() {
    if (note_finished_handled || !is_note_finished()) {
        return false;
    }
    note_finished_handled = true;
    return true;
}
