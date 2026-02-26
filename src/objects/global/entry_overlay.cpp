#include "entry_overlay.h"

EntryOverlay::EntryOverlay() : online(get_config().general.fake_online) {
}

void EntryOverlay::update(double current_ms) {
}

void EntryOverlay::draw(float x, float y) {
    int frame = online ? 1 : 0;
    global_tex.draw_texture("overlay", "banapass_or", {.frame=frame, .x=x, .y=y});
    global_tex.draw_texture("overlay", "banapass_card", {.frame=frame, .x=x, .y=y});
    global_tex.draw_texture("overlay", "banapass_osaifu_keitai", {.frame=frame, .x=x, .y=y});
    if (!online) {
        global_tex.draw_texture("overlay", "banapass_no", {.frame=frame, .x=x, .y=y});
    }
    global_tex.draw_texture("overlay", "camera", {.frame=frame, .x=x, .y=y});
}
