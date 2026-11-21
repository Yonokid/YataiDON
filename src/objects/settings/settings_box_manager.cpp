#include "settings_box_manager.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

#include "../../libs/audio/audio.h"
#include "../../libs/time.h"

// Same 1280x720-canvas base values as SettingsBox; scale with the skin.
static constexpr float BOX_STEP_BASE = 100.0f;
static constexpr float BOX_INITIAL_Y_BASE = -50.0f;
static constexpr float TAP_SLOP_BASE = 20.0f;
static constexpr float SWIPE_MIN_BASE = 80.0f;
static constexpr float VALUE_DRAG_STEP_BASE = 40.0f;
// A flick decays to 1/e of its speed every FLING_DECAY_MS; holding still this long before lifting cancels it.
static constexpr double FLING_DECAY_MS = 325.0;
static constexpr double FLING_HOLD_MS = 80.0;
static constexpr float FLING_STOP_BASE = 0.02f; // px per ms

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
        fling_velocity = 0.0f;
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
        fling_velocity = 0.0f;
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

bool SettingsBoxManager::touch(Vec2 start, Vec2 pos, bool down) {
    if (boxes.empty()) {
        return false;
    }
    touch_mode = true;
    const double now = get_current_ms();
    if (!drag || drag->start != start) {
        drag =
            Drag{.start = start, .scrolled_y = start.y, .moved_ms = now, .from_list = start.x < boxes.front()->right()};
        fling_velocity = 0.0f;
    }
    const float s = tex.screen_scale;
    const float dx = pos.x - start.x;
    const float dy = pos.y - start.y;
    drag->moved = drag->moved || std::hypot(dx, dy) > TAP_SLOP_BASE * s;
    if (drag->moved && !editing()) {
        const float list_dy = pos.y - drag->scrolled_y;
        if (drag->from_list && list_dy != 0.0f && scroll_list(list_dy)) {
            const double dt = std::max(now - drag->moved_ms, 1.0);
            drag->velocity = (0.6f * static_cast<float>(list_dy / dt)) + (0.4f * drag->velocity);
            drag->scrolled_y = pos.y;
            drag->moved_ms = now;
        }
    } else if (drag->moved && !drag->from_list && boxes[selected_box_index]->scroll_option(pos.y - drag->scrolled_y)) {
        drag->scrolled_y = pos.y;
    } else if (drag->moved && !drag->from_list) {
        const int steps = static_cast<int>(dx / (VALUE_DRAG_STEP_BASE * s));
        for (; drag->value_steps < steps; drag->value_steps++) {
            audio.play_sound("kat", VolumePreset::HITSOUND);
            move_right();
        }
        for (; drag->value_steps > steps; drag->value_steps--) {
            audio.play_sound("kat", VolumePreset::HITSOUND);
            move_left();
        }
    }
    if (down) {
        return false;
    }
    const Drag ended = *drag;
    drag.reset();
    if (ended.from_list && ended.moved && !editing() && now - ended.moved_ms < FLING_HOLD_MS) {
        fling_velocity = ended.velocity;
        last_update_ms = now;
    }
    if (!ended.moved) {
        audio.play_sound("don", VolumePreset::HITSOUND);
        return tap(start);
    }
    if (!ended.from_list && box_selected && ended.value_steps == 0 && !editing() && std::abs(dx) > SWIPE_MIN_BASE * s &&
        std::abs(dx) > std::abs(dy)) {
        audio.play_sound("kat", VolumePreset::HITSOUND);
        dx < 0 ? move_right() : move_left();
    }
    return false;
}

// The highlight stays on whichever box passes the keyboard selection slot.
bool SettingsBoxManager::scroll_list(float dy) {
    if (!std::ranges::all_of(boxes, [](const auto& b) { return b->can_move(); })) {
        return false;
    }
    for (auto& b : boxes) {
        b->scroll_by(dy);
    }
    if (box_selected) {
        return true;
    }
    const float slot = (BOX_INITIAL_Y_BASE + (INITIAL_SELECTED * BOX_STEP_BASE)) * tex.screen_scale;
    float best = std::numeric_limits<float>::infinity();
    for (int i = 0; i < std::ssize(boxes); i++) {
        const float dist = std::abs(boxes[i]->top() - slot);
        if (dist < best) {
            best = dist;
            selected_box_index = i;
        }
    }
    return true;
}

bool SettingsBoxManager::tap(Vec2 world) {
    const bool locked = editing();
    for (int i = 0; i < std::ssize(boxes); i++) {
        if (!boxes[i]->contains(world)) {
            continue;
        }
        if (locked || (box_selected && i == selected_box_index)) {
            return false;
        }
        if (box_selected) {
            boxes[selected_box_index]->in_box = false;
            box_selected = false;
        }
        selected_box_index = i;
        return select_box();
    }
    if (!box_selected) {
        return false;
    }
    switch (boxes[selected_box_index]->tap(world)) {
    case BoxTap::Left:
        move_left();
        break;
    case BoxTap::Right:
        move_right();
        break;
    case BoxTap::None:
        break;
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
    if (fling_velocity != 0.0f) {
        const double dt = current_time_ms - last_update_ms;
        last_update_ms = current_time_ms;
        if (editing() || !scroll_list(static_cast<float>(fling_velocity * dt))) {
            fling_velocity = 0.0f;
        } else {
            fling_velocity *= static_cast<float>(std::exp(-dt / FLING_DECAY_MS));
            if (std::abs(fling_velocity) < FLING_STOP_BASE * tex.screen_scale) {
                fling_velocity = 0.0f;
            }
        }
    }
    for (int i = 0; i < std::ssize(boxes); i++) {
        bool selected = (i == selected_box_index) && (box_selected || !touch_mode);
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
