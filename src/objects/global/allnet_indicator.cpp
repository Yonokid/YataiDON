#include "allnet_indicator.h"
#include "../../libs/config.h"
#include "../../libs/texture.h"

AllNetIcon::AllNetIcon() : online(get_config().general.fake_online) {
}

void AllNetIcon::update(double current_ms) {

}

void AllNetIcon::draw(float x, float y) {
    int frame;
    if (online) {
        frame = 2;
    } else {
        frame = 0;
    }
    global_tex.draw_texture(OVERLAY::ALLNET_INDICATOR, {.frame=frame, .x=x, .y=y});
}
