#include "coin_overlay.h"

CoinOverlay::CoinOverlay() {
    free_play = new OutlinedText(global_tex.skin_config["free_play"].text[get_config().general.language], global_tex.skin_config["free_play"].font_size, ray::WHITE, ray::BLACK);
}

void CoinOverlay::update(double current_ms) {
}

void CoinOverlay::draw(float x, float y) {
    free_play->draw({.x=(float)global_tex.screen_width / 2 - free_play->width / 2, .y=global_tex.skin_config["free_play"].y});
}
