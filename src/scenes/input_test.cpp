#include "input_test.h"
#include "../libs/input.h"

void InputTestScreen::on_screen_start() {
    // Loads the screen's sounds (don/kat come from the skin root), which the
    // hit handlers below play. Without it the screen was silent - the
    // previous screen's on_screen_end had already unloaded everything.
    Screen::on_screen_start();
    tex.load_animations("game");
    tex.load_folder("game", "practice");
    tex.load_folder("settings", "background");

    t_background = tex.get_texture("background/background");
    t_large_drum = tex.get_texture("practice/large_drum");

    // Centre the drum by its drawn size (x2/y2: a skin may give it a size other than the PNG's).
    if (!t_large_drum) return;
    auto& drum = *t_large_drum;
    drum_x_offset = (tex.screen_width - drum.x2[0]) / 2.0f - drum.x[0];
    drum_y_offset = (tex.screen_height - drum.y2[0]) / 2.0f - drum.y[0];
}

std::optional<Screens> InputTestScreen::update() {
    if (auto result = Screen::update()) {
        return result;
    }

    if (check_key_pressed(global_data.config->keys.back_key)) {
        return on_screen_end(Screens::SETTINGS);
    }

    double current_ms = get_current_ms();
    for (auto it = hit_effects.begin(); it != hit_effects.end();) {
        (*it)->update(current_ms);
        if ((*it)->is_finished()) it = hit_effects.erase(it);
        else ++it;
    }

    float strength = 1.0f;
    if (is_l_don_pressed(PlayerNum::ALL, &strength)) {
        hit_effects.push_back(std::make_unique<InputTestDrumEffect>(DrumType::DON, Side::LEFT, drum_x_offset, drum_y_offset));
        audio.play_sound("don", VolumePreset::SOUND, strength);
    }

    if (is_r_don_pressed(PlayerNum::ALL, &strength)) {
        hit_effects.push_back(std::make_unique<InputTestDrumEffect>(DrumType::DON, Side::RIGHT, drum_x_offset, drum_y_offset));
        audio.play_sound("don", VolumePreset::SOUND, strength);
    }

    if (is_l_kat_pressed(PlayerNum::ALL, &strength)) {
        hit_effects.push_back(std::make_unique<InputTestDrumEffect>(DrumType::KAT, Side::LEFT, drum_x_offset, drum_y_offset));
        audio.play_sound("kat", VolumePreset::SOUND, strength);
    }

    if (is_r_kat_pressed(PlayerNum::ALL, &strength)) {
        hit_effects.push_back(std::make_unique<InputTestDrumEffect>(DrumType::KAT, Side::RIGHT, drum_x_offset, drum_y_offset));
        audio.play_sound("kat", VolumePreset::SOUND, strength);
    }

    return std::nullopt;
}

void InputTestScreen::draw() {
    tex.draw_texture(t_background);
    tex.draw_texture(t_large_drum, {.x = drum_x_offset, .y = drum_y_offset});
    for (auto& effect : hit_effects) {
        effect->draw(0);
    }
}
