#include "tournament_attract.h"

TournamentAttract::TournamentAttract(double current_ms) {
    if (!load("TournamentAttract", "tournament_attract", current_ms)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_draw = lua_object["draw"];
    fn_is_finished = lua_object["is_finished"];
    fn_can_spin = lua_object["can_spin"];
    fn_spin = lua_object["spin"];
    fn_close = lua_object["close"];
}

TournamentAttract::~TournamentAttract() {
    call(fn_close, "TournamentAttract:close");
}

void TournamentAttract::update(double current_ms) {
    call(fn_update, "TournamentAttract:update", current_ms);
}

void TournamentAttract::draw() {
    call(fn_draw, "TournamentAttract:draw");
}

bool TournamentAttract::is_finished() {
    return call_r<bool>(fn_is_finished, "TournamentAttract:is_finished").value_or(true);
}

bool TournamentAttract::can_spin() {
    return call_r<bool>(fn_can_spin, "TournamentAttract:can_spin").value_or(false);
}

void TournamentAttract::spin(int puchichara) {
    call(fn_spin, "TournamentAttract:spin", puchichara);
}
