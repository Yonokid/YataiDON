#include "box_manager.h"

#include "../../libs/gfx/text.h"
#include "../../libs/global_data.h"

static constexpr int ENTRY_FADE_OUT_ANIM_ID = 9;

BoxManager::BoxManager(bool two_player) : is_2p(two_player) {
    if (!global_data.config) {
        throw std::runtime_error("BoxManager: global_data.config not initialized");
    }

    fade_out = dynamic_cast<FadeAnimation*>(tex.get_animation(ENTRY_FADE_OUT_ANIM_ID));
    if (!fade_out) {
        throw std::runtime_error("BoxManager: animation " + std::to_string(ENTRY_FADE_OUT_ANIM_ID) +
                                 " is not a FadeAnimation");
    }

    if (!tex.options[SCO::DISABLE_WAIWAI]) {
        waiwai_label = font_manager.get_text_label("entry_waiwai");
    }
    build_board_list();
}

bool BoxManager::waiwai_replaces_game() const {
    const auto is_guest = [](const std::string& code) { return code.empty() || code == "0" || code == "1"; };
    const auto& network = global_data.config->network;
    return is_2p && !waiwai_label.empty() && is_guest(network.access_code_1) && is_guest(network.access_code_2);
}

void BoxManager::build_board_list() {
    int font_size = tex.get_skin_config(SC::ENTRY_BOX_TEXT)->font_size;

    if (script_manager.lua) {
        (*script_manager.lua)["__hss_entry_reset"] = true;
    }

    boxes.clear();
    box_locations.clear();

    dan_box_index = (dan_available && !is_2p) ? 1 : -1;

    waiwai_board = waiwai_replaces_game();
    if (waiwai_board) {
        box_locations = {Screens::WAIWAI_SONG_SELECT};
        boxes.push_back(std::make_unique<Box>(waiwai_label, font_size, Screens::WAIWAI_SONG_SELECT));
    } else {
        box_locations = {Screens::SONG_SELECT};
        boxes.push_back(
            std::make_unique<Box>(font_manager.get_text_label("entry_game"), font_size, Screens::SONG_SELECT));
    }
    if (dan_box_index >= 0) {
        box_locations.push_back(Screens::DAN_SELECT);
        boxes.push_back(std::make_unique<Box>(dan_text, font_size, Screens::DAN_SELECT));
    }
    for (auto [screen, sc] :
         {std::pair{Screens::AI_SELECT, "entry_ai_battle"}, std::pair{Screens::PRACTICE_SELECT, "entry_practice"},
          std::pair{Screens::ENTRY, "entry_costume"}, std::pair{Screens::REWARD_SHOP, "entry_shop"},
          std::pair{Screens::SETTINGS, "entry_settings"}}) {
        // no script, no shop board
        if (screen == Screens::REWARD_SHOP &&
            (tex.options[SCO::DISABLE_REWARD_SHOP] || !script_manager.has_lua_script("reward_shop"))) {
            continue;
        }
        if (screen == Screens::AI_SELECT && !script_manager.has_lua_script("ai_support_menu")) {
            continue;
        }
        // like the dan board: a skin that blanks the board's text leaves the board out
        std::string label = font_manager.get_text_label(sc);
        if (label.empty()) {
            continue;
        }
        // no 3D characters: nothing to dress up, so no costume board
        if (screen == Screens::ENTRY && tex.options[SCO::DISABLE_CHARA_3D]) {
            continue;
        }
        box_locations.push_back(screen);
        boxes.push_back(std::make_unique<Box>(label, font_size, screen));
    }

    num_boxes = boxes.size();

    if (is_2p) {
        for (int i = 0; i < num_boxes; i++) {
            if (box_locations[i] == Screens::SONG_SELECT) {
                boxes[i]->location = Screens::SONG_SELECT_2P;
            }
        }
    }

    float spacing_x = tex.get_skin_config(SC::ENTRY_BOX_SPACING)->x;
    float spacing_y = tex.get_skin_config(SC::ENTRY_BOX_SPACING)->y;
    is_vertical = spacing_y > 0;

    if (is_vertical) {
        float step = spacing_y;
        float total_height = (num_boxes - 1) * step;
        float start_y = tex.screen_height / 2.0f - total_height / 2.0f;
        float center_x = tex.screen_width / 2.0f - tex.textures["mode_select/box_highlight_center"]->width / 2.0f;

        for (int i = 0; i < num_boxes; i++) {
            boxes[i]->set_positions(center_x, start_y + i * step);
            if (i > 0) {
                boxes[i]->move_down();
            }
        }
    } else {
        float box_width = boxes[0]->width;
        float total_width = num_boxes * box_width + (num_boxes - 1) * spacing_x;
        float start_x = tex.screen_width / 2.0f - total_width / 2.0f;

        for (int i = 0; i < num_boxes; i++) {
            boxes[i]->set_positions(start_x + i * (box_width + spacing_x));
            if (i > 0) {
                boxes[i]->move_right();
            }
        }
    }
}

bool BoxManager::check_board_list_change() const {
    return (dan_available && !is_2p) != (dan_box_index >= 0) || waiwai_replaces_game() != waiwai_board;
}

void BoxManager::change_board_list() {
    build_board_list();
    selected_box_index = 0;
}

// practice and AI battle have no 2P mode: with two players in, their boards are greyed out
bool BoxManager::is_disabled(int index) const {
    const Screens location = boxes[index]->location;
    return is_2p && (location == Screens::PRACTICE_SELECT || location == Screens::AI_SELECT);
}

bool BoxManager::selection_allowed() const {
    return !is_disabled(selected_box_index) && !(is_2p && boxes[selected_box_index]->location == Screens::DAN_SELECT);
}

void BoxManager::select_box() {
    const Screens location = boxes[selected_box_index]->location;
    if (location == Screens::PRACTICE_SELECT) {
        bncoin_request = true;
        return;
    }
    if (location == Screens::REWARD_SHOP) {
        shop_request = true;
        return;
    }
    if (!selection_allowed()) {
        audio.play_sound("kat", VolumePreset::SOUND);
        return;
    }
    if (location == Screens::SONG_SELECT) {
        audio.play_sound("mode_game", VolumePreset::VOICE);
    } else if (location == Screens::AI_SELECT) {
        audio.play_sound("mode_ai", VolumePreset::VOICE);
    }
    fade_out->start();
}

void BoxManager::start_selected() {
    fade_out->start();
}

bool BoxManager::is_box_selected() const {
    return fade_out->is_started;
}

bool BoxManager::is_finished() const {
    return fade_out->is_finished;
}

bool BoxManager::is_costume_box() const {
    return boxes[selected_box_index]->location == Screens::ENTRY;
}

void BoxManager::open_costume_menu(PlayerNum player_num) {
    costume_menu_open = true;
    opening_player = player_num;
}

Screens BoxManager::selected_box() const {
    return boxes[selected_box_index]->location;
}

void BoxManager::move_left() {
    int prev_selection = selected_box_index;
    if (boxes[prev_selection]->move->is_started && !boxes[prev_selection]->move->is_finished) {
        return;
    }
    selected_box_index = std::max(0, selected_box_index - 1);
    if (prev_selection == selected_box_index) {
        return;
    }
    if (is_vertical) {
        boxes[selected_box_index + 1]->move_down();
        boxes[selected_box_index]->move_down();
    } else {
        boxes[selected_box_index + 1]->move_right();
        boxes[selected_box_index]->move_right();
    }
}

void BoxManager::move_right() {
    int prev_selection = selected_box_index;
    if (boxes[prev_selection]->move->is_started && !boxes[prev_selection]->move->is_finished) {
        return;
    }
    selected_box_index = std::min(num_boxes - 1, selected_box_index + 1);
    if (prev_selection == selected_box_index) {
        return;
    }
    if (is_vertical) {
        boxes[selected_box_index - 1]->move_up();
        boxes[selected_box_index]->move_up();
    } else {
        if (selected_box_index != 0) {
            boxes[selected_box_index - 1]->move_left();
        }
        boxes[selected_box_index]->move_left();
    }
}

void BoxManager::update(double current_time_ms, bool two_player) {
    this->is_2p = two_player;
    if (!fade_out->is_started && check_board_list_change()) {
        change_board_list();
    }
    for (int i = 0; i < num_boxes; i++) {
        if (box_locations[i] == Screens::SONG_SELECT) {
            boxes[i]->location = this->is_2p ? Screens::SONG_SELECT_2P : Screens::SONG_SELECT;
        }
    }
    fade_out->update(current_time_ms);
    for (int i = 0; i < num_boxes; i++) {
        boxes[i]->update(current_time_ms, i == selected_box_index);
    }
}

void BoxManager::draw() {
    for (int i = 0; i < num_boxes; i++) {
        boxes[i]->draw(fade_out->attribute, i > 0, i < num_boxes - 1, is_disabled(i));
    }
}
