#include "entry_overlay.h"
#include "../../libs/config.h"
#include "../../libs/texture.h"

EntryOverlay::EntryOverlay() : online(get_config().general.fake_online) {
}

void EntryOverlay::update(double current_ms) {
}

void EntryOverlay::draw(float x, float y) {
    int frame = online ? 1 : 0;
    global_tex.draw_texture(OVERLAY::BANAPASS_OR, {.frame=frame, .x=x, .y=y});
    global_tex.draw_texture(OVERLAY::BANAPASS_CARD, {.frame=frame, .x=x, .y=y});
    global_tex.draw_texture(OVERLAY::BANAPASS_OSAIFU_KEITAI, {.frame=frame, .x=x, .y=y});
    if (!online) {
        global_tex.draw_texture(OVERLAY::BANAPASS_NO, {.frame=frame, .x=x, .y=y});
    }
    global_tex.draw_texture(OVERLAY::CAMERA, {.frame=frame, .x=x, .y=y});
}
