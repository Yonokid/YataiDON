#include "settings_box_manager.h"

static constexpr int   INITIAL_SELECTED = 3;
static constexpr float BOX_STEP         = 100.0f;
static constexpr float BOX_INITIAL_Y    = -50.0f;

SettingsBoxManager::SettingsBoxManager(const rapidjson::Document& tmpl)
    : num_boxes(0)
    , selected_box_index(INITIAL_SELECTED)
    , box_selected(false)
{
    std::string lang = global_data.config->general.language;

    for (auto it = tmpl.MemberBegin(); it != tmpl.MemberEnd(); ++it) {
        std::string cat_name = it->name.GetString();
        const auto& cat = it->value;

        std::string label;
        if (cat.HasMember("name")) {
            const auto& n = cat["name"];
            if (n.HasMember(lang.c_str())) label = n[lang.c_str()].GetString();
            else if (n.HasMember("en"))    label = n["en"].GetString();
        }

        const rapidjson::Value* opts =
            cat.HasMember("options") ? &cat["options"] : nullptr;
        rapidjson::Value empty_obj(rapidjson::kObjectType);
        if (!opts) opts = &empty_obj;

        boxes.push_back(new SettingsBox(cat_name, label, *opts));
    }

    num_boxes = (int)boxes.size();

    for (int i = 0; i < num_boxes; i++) {
        boxes[i]->set_y(BOX_INITIAL_Y + i * BOX_STEP);
    }

    selected_box_index = std::min(selected_box_index, num_boxes - 1);
}

SettingsBoxManager::~SettingsBoxManager() {
    for (auto* b : boxes) delete b;
}

void SettingsBoxManager::move_left() {
    if (box_selected) {
        SettingsBox* box = boxes[selected_box_index];
        box_selected = box->move_option_left();
    } else {
        bool moved = true;
        for (auto* b : boxes) {
            if (!b->move_left()) moved = false;
        }
        if (moved) {
            selected_box_index = (selected_box_index - 1 + num_boxes) % num_boxes;
        }
    }
}

void SettingsBoxManager::move_right() {
    if (box_selected) {
        boxes[selected_box_index]->move_option_right();
    } else {
        for (auto* b : boxes) b->move_right();
        selected_box_index = (selected_box_index + 1) % num_boxes;
    }
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
    for (int i = 0; i < num_boxes; i++) {
        bool selected = (i == selected_box_index) && !box_selected;
        boxes[i]->update(current_time_ms, selected);
    }
}

void SettingsBoxManager::draw() {
    for (auto* b : boxes) b->draw();
}
