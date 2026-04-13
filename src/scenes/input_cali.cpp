#include "input_cali.h"

void InputCaliScreen::on_screen_start() {
    Screen::on_screen_start();
}

Screens InputCaliScreen::on_screen_end(Screens next_screen) {
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> InputCaliScreen::update() {
    Screen::update();

    if (ray::IsKeyPressed(global_data.config->keys.back_key)) {
        return on_screen_end(Screens::SETTINGS);
    }

    return std::nullopt;
}

void InputCaliScreen::draw() {
    ray::ClearBackground(ray::BLACK);
}
