#include "box_manager.h"

BoxManager::BoxManager() : selected_box_index(0), is_2p(false) {
    box_locations = {Screens::SONG_SELECT, Screens::PRACTICE_SELECT, Screens::AI_SELECT, Screens::SETTINGS};

    std::string lang = global_data.config->general.language;
    auto& skin = tex.skin_config;
    int font_size = skin[SC::ENTRY_BOX_TEXT].font_size;

    auto make_text = [&](SC key) {
        return std::make_unique<OutlinedText>(skin[key].text[lang], font_size, ray::WHITE, ray::Color(109, 68, 24, 255), true, 5);
    };
    boxes.push_back(std::make_unique<Box>(make_text(SC::ENTRY_GAME),       box_locations[0]));
    boxes.push_back(std::make_unique<Box>(make_text(SC::ENTRY_PRACTICE),   box_locations[1]));
    boxes.push_back(std::make_unique<Box>(make_text(SC::ENTRY_AI_BATTLE),  box_locations[2]));
    boxes.push_back(std::make_unique<Box>(make_text(SC::ENTRY_SETTINGS),   box_locations[3]));

    num_boxes = boxes.size();

    fade_out = (FadeAnimation*)tex.get_animation(9);

    float spacing = skin[SC::ENTRY_BOX_SPACING].x;
    float box_width = boxes[0]->width;
    float total_width = num_boxes * box_width + (num_boxes - 1) * spacing;
    float start_x = tex.screen_width / 2.0f - total_width / 2.0f;

    for (int i = 0; i < num_boxes; i++) {
        boxes[i]->set_positions(start_x + i * (box_width + spacing));
        if (i > 0) {
            boxes[i]->move_right();
        }
    }
}

void BoxManager::select_box() {
    fade_out->start();
}

bool BoxManager::is_box_selected() {
    return fade_out->is_started;
}

bool BoxManager::is_finished() {
    return fade_out->is_finished;
}

Screens BoxManager::selected_box() {
    return boxes[selected_box_index]->location;
}

void BoxManager::move_left() {
    int prev_selection = selected_box_index;
    if (boxes[prev_selection]->move->is_started && !boxes[prev_selection]->move->is_finished) {
        return;
    }
    selected_box_index = std::max(0, selected_box_index - 1);
    if (prev_selection == selected_box_index) return;
    if (selected_box_index != selected_box_index - 1) {
        boxes[selected_box_index + 1]->move_right();
    }
    boxes[selected_box_index]->move_right();
}

void BoxManager::move_right() {
    int prev_selection = selected_box_index;
    if (boxes[prev_selection]->move->is_started && !boxes[prev_selection]->move->is_finished) {
        return;
    }
    selected_box_index = std::min(num_boxes - 1, selected_box_index + 1);
    if (prev_selection == selected_box_index) return;
    if (selected_box_index != 0) {
        boxes[selected_box_index - 1]->move_left();
    }
    boxes[selected_box_index]->move_left();
}

void BoxManager::update(double current_time_ms, bool is_2p) {
    this->is_2p = is_2p;
    if (this->is_2p) {
        box_locations = {Screens::SONG_SELECT_2P, Screens::PRACTICE_SELECT, Screens::AI_SELECT, Screens::SETTINGS};
        for (int i = 0; i < num_boxes; i++) {
            boxes[i]->location = box_locations[i];
        }
    }
    fade_out->update(current_time_ms);
    for (int i = 0; i < num_boxes; i++) {
        boxes[i]->update(current_time_ms, i == selected_box_index);
    }
}

void BoxManager::draw() {
    for (auto& box : boxes) {
        box->draw(fade_out->attribute);
    }
}
