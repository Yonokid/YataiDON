#include "settings_box_manager.h"

#include <iterator>

// Same 1280x720-canvas base values as SettingsBox; scale with the skin.
static constexpr float BOX_STEP_BASE = 100.0f;
static constexpr float BOX_INITIAL_Y_BASE = -50.0f;

SettingsBoxManager::SettingsBoxManager(const rapidjson::Document& tmpl) {
    std::string lang = global_data.config->general.language;

    for (auto it = tmpl.MemberBegin(); it != tmpl.MemberEnd(); ++it) {
        std::string cat_name = it->name.GetString();
        const auto& cat = it->value;

        std::string label;
        if (cat.HasMember("name")) {
            const auto& n = cat["name"];
            if (n.HasMember(lang.c_str())) {
                label = n[lang.c_str()].GetString();
            } else if (n.HasMember("en")) {
                label = n["en"].GetString();
            }
        }

        const rapidjson::Value* opts = cat.HasMember("options") ? &cat["options"] : nullptr;
        rapidjson::Value empty_obj(rapidjson::kObjectType);
        if (!opts) {
            opts = &empty_obj;
        }

        boxes.push_back(std::make_unique<SettingsBox>(cat_name, label, *opts));
    }

    int num_boxes = static_cast<int>(boxes.size());
    for (int i = 0; i < num_boxes; i++) {
        boxes[i]->set_box_count(num_boxes);
        boxes[i]->set_y((BOX_INITIAL_Y_BASE + i * BOX_STEP_BASE) * tex.screen_scale);
    }

    if (num_boxes == 0) {
        return;
    }

    selected_box_index = std::min(selected_box_index, num_boxes - 1);
}

SettingsBoxManager::~SettingsBoxManager() = default;

void SettingsBoxManager::move_left() {
    if (box_selected) {
        box_selected = boxes[selected_box_index]->move_option_left();
    } else {
        // All-or-nothing: only start moving the carousel once every box is
        // idle, so a box still animating from a previous move can't leave
        // the others (and the selection index) out of sync.
        for (auto& b : boxes) {
            if (!b->can_move()) {
                return;
            }
        }
        for (auto& b : boxes) {
            b->move_left();
        }
        int num_boxes = static_cast<int>(boxes.size());
        selected_box_index = (selected_box_index - 1 + num_boxes) % num_boxes;
    }
}

void SettingsBoxManager::move_right() {
    if (box_selected) {
        boxes[selected_box_index]->move_option_right();
    } else {
        for (auto& b : boxes) {
            if (!b->can_move()) {
                return;
            }
        }
        for (auto& b : boxes) {
            b->move_right();
        }
        selected_box_index = (selected_box_index + 1) % static_cast<int>(boxes.size());
    }
}

std::optional<Screens> SettingsBoxManager::pending_screen_change() {
    for (auto& b : boxes) {
        auto result = b->pending_screen_change();
        if (result) {
            return result;
        }
    }
    return std::nullopt;
}

bool SettingsBoxManager::tap(Vec2 world) {
    if (box_selected) {
        const float third = tex.screen_width / 3.0f;
        if (world.x < third) {
            move_left();
        } else if (world.x > 2.0f * third) {
            move_right();
        } else {
            return select_box();
        }
        return false;
    }
    const float offset = world.y - boxes[selected_box_index]->center_y();
    const float half_step = BOX_STEP_BASE * tex.screen_scale / 2.0f;
    if (offset < -half_step) {
        move_left();
    } else if (offset > half_step) {
        move_right();
    } else {
        return select_box();
    }
    return false;
}

bool SettingsBoxManager::select_box() {
    if (boxes[selected_box_index]->box_name == "exit") {
        return true;
    }
    if (box_selected) {
        boxes[selected_box_index]->select_option();
    } else {
        box_selected = true;
        boxes[selected_box_index]->select();
    }
    return false;
}

void SettingsBoxManager::update(double current_time_ms) {
    for (int i = 0; i < std::ssize(boxes); i++) {
        bool selected = (i == selected_box_index) && !box_selected;
        boxes[i]->update(current_time_ms, selected);
    }
}

void SettingsBoxManager::draw() {
    for (auto& b : boxes) {
        b->draw();
    }
    for (auto& b : boxes) {
        b->draw_overlay();
    }
}
