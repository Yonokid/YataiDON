#include "attract_announce.h"

AttractAnnounce::AttractAnnounce(const std::string& mode) {
    if (!load("AttractAnnounce", "attract_announce", mode)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_draw = lua_object["draw"];
}

void AttractAnnounce::update(double current_ms) {
    call(fn_update, "AttractAnnounce:update", current_ms);
}
void AttractAnnounce::draw(float fade) {
    call(fn_draw, "AttractAnnounce:draw", fade);
}
