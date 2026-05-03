#include "coin_overlay.h"
#include "../../libs/config.h"

CoinOverlay::CoinOverlay() {
    free_play = std::make_unique<OutlinedText>(global_tex.skin_config[SC::FREE_PLAY].text[get_config().general.language], global_tex.skin_config[SC::FREE_PLAY].font_size, ray::WHITE, ray::BLACK, false, 4, 5);
}

void CoinOverlay::update(double current_ms) {
}

void CoinOverlay::draw(float x, float y) {
    free_play->draw({.x=(float)global_tex.screen_width / 2 - free_play->width / 2, .y=global_tex.skin_config[SC::FREE_PLAY].y});
}
