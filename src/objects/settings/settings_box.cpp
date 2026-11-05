#include "settings_box.h"

#include <iterator>
#include <stdexcept>
#include <utility>

#include "../../libs/audio/audio.h"
#include "../../libs/file/filesystem.h"
#include "../../libs/input/input.h"
#include "../../platform/platform.h"

std::unique_ptr<BaseOptionBox> SettingsBox::make_option_box(const rapidjson::Value& opt) {
    if (!opt.HasMember("type") || !opt["type"].IsString() || !opt.HasMember("path") || !opt["path"].IsString()) {
        throw std::runtime_error("option missing 'type' or 'path'");
    }
    std::string type = opt["type"].GetString();
    std::string path = opt["path"].GetString();
    std::string lang = global_data.config->general.language;

    auto localise = [&](const rapidjson::Value& obj) -> std::string {
        if (obj.HasMember(lang.c_str())) {
            return obj[lang.c_str()].GetString();
        }
        if (obj.HasMember("en")) {
            return obj["en"].GetString();
        }
        return "";
    };

    std::string name = opt.HasMember("name") ? localise(opt["name"]) : "";
    std::string desc = opt.HasMember("description") ? localise(opt["description"]) : "";

    std::map<std::string, std::string> values_map;
    if (opt.HasMember("values") && opt["values"].IsObject()) {
        for (auto it = opt["values"].MemberBegin(); it != opt["values"].MemberEnd(); ++it) {
            if (platform_offers_setting(path, it->name.GetString())) {
                values_map[it->name.GetString()] = localise(it->value);
            }
        }
    }

    if (type == "audiooffset") {
        return std::make_unique<AudioOffsetOptionBox>(name, desc, path, Screens::INPUT_CALI);
    }
    if (type == "bool") {
        std::string true_label = values_map.contains("true") ? values_map["true"] : "Enabled";
        std::string false_label = values_map.contains("false") ? values_map["false"] : "Disabled";
        return std::make_unique<BoolOptionBox>(name, desc, path, true_label, false_label);
    }
    if (type == "int") {
        return std::make_unique<IntOptionBox>(name, desc, path, values_map);
    }
    if (type == "string") {
        return std::make_unique<StrOptionBox>(name, desc, path, values_map);
    }
    if (type == "account") {
        return std::make_unique<AccountOptionBox>(name, desc, path);
    }
    if (type == "skin") {
        // Skin choices come from what's actually installed, not the json
        // template -- it can't know that ahead of time.
        values_map.clear();
        for (const auto& skin : list_available_skins()) {
            values_map[skin] = skin;
        }
        return std::make_unique<StrOptionBox>(name, desc, path, values_map);
    }
    if (type == "audiodevice") {
        const int device_type = global_data.config->audio.device_type;
        for (const auto& device : enumerate_audio_devices(device_type)) {
            if (!device.empty()) {
                values_map[device] = device;
            }
        }
        return std::make_unique<StrOptionBox>(name, desc, path, values_map);
    }
    if (type == "keybind") {
        return std::make_unique<KeybindOptionBox>(name, desc, path);
    }
    if (type == "keybind_controller") {
        return std::make_unique<KeyBindControllerOptionBox>(name, desc, path);
    }
    if (type == "float") {
        return std::make_unique<FloatOptionBox>(name, desc, path);
    }
    throw std::runtime_error("unknown option type '" + type + "'");
}

// Laid out for the 1280x720 base canvas; scale with the skin so box
// movement and label size stay aligned with the (already-scaled) box art.
static constexpr float WRAP_TOP_BASE = -50.0f;
static constexpr float BOX_STEP_BASE = 100.0f;
static constexpr int LABEL_FONT_SIZE_BASE = 35;
static constexpr float LABEL_OUTLINE_BASE = 5.0f;

static float wrap_top() {
    return WRAP_TOP_BASE * tex.screen_scale;
}
static float box_step() {
    return BOX_STEP_BASE * tex.screen_scale;
}

SettingsBox::SettingsBox(std::string name, const std::string& label_text, const rapidjson::Value& options_json)
    : box_name(std::move(name)),
      label(std::make_unique<OutlinedText>(label_text, static_cast<int>(LABEL_FONT_SIZE_BASE * tex.screen_scale),
                                           Color::WHITE, Color{109, 68, 24, 255}, false, LABEL_OUTLINE_BASE)),
      x(10.0f * tex.screen_scale), y(wrap_top()), start_position(wrap_top()),
      wrap_bottom(wrap_top()) // corrected by set_box_count() once the manager knows the total
      ,
      target_position(std::numeric_limits<float>::infinity()) {
    move_anim = tex.get_animation(0, true);
    blue_arrow_fade = dynamic_cast<FadeAnimation*>(tex.get_animation(1, true));
    blue_arrow_move = dynamic_cast<MoveAnimation*>(tex.get_animation(2, true));
    if (!blue_arrow_fade || !blue_arrow_move) {
        throw std::runtime_error("SettingsBox: animation 1/2 has an unexpected type");
    }

    t_box = tex.get_texture("box/box");
    t_box_highlight = tex.get_texture("box/box_highlight");
    t_blue_arrow = tex.get_texture("background/blue_arrow");

    if (options_json.IsObject()) {
        for (auto it = options_json.MemberBegin(); it != options_json.MemberEnd(); ++it) {
            const auto& opt = it->value;
            if (opt.IsObject() && opt.HasMember("path") && opt["path"].IsString() &&
                !platform_offers_setting(opt["path"].GetString(), "")) {
                continue;
            }
            try {
                options.push_back(make_option_box(it->value));
            } catch (const std::exception& e) {
                spdlog::warn("Skipping option '{}': {}", it->name.GetString(), e.what());
            }
        }
    }
}

SettingsBox::~SettingsBox() = default;

void SettingsBox::set_y(float new_y) {
    y = new_y;
    start_position = new_y;
    target_position = std::numeric_limits<float>::infinity();
}

void SettingsBox::set_box_count(int box_count) {
    if (box_count <= 0) {
        return;
    }
    wrap_bottom = wrap_top() + box_count * box_step();
}

bool SettingsBox::can_move() const {
    return y == target_position || std::isinf(target_position);
}

bool SettingsBox::move_left() {
    if (!can_move()) {
        return false;
    }
    move_anim->start();
    direction = 1;
    start_position = y;
    target_position = y + box_step() * direction;
    if (target_position >= wrap_bottom) {
        target_position = wrap_top() + (target_position - wrap_bottom);
    }
    return true;
}

bool SettingsBox::move_right() {
    if (!can_move()) {
        return false;
    }
    move_anim->start();
    direction = -1;
    start_position = y;
    target_position = y + box_step() * direction;
    if (target_position < wrap_top()) {
        target_position = wrap_bottom + (target_position - wrap_top());
    }
    return true;
}

bool SettingsBox::move_option_left() {
    if (!options.empty() && options[option_index]->is_highlighted) {
        options[option_index]->move_left();
        return true;
    }
    if (option_index == 0) {
        in_box = false;
        return false;
    }
    option_index--;
    return true;
}

void SettingsBox::move_option_right() {
    if (options.empty()) {
        return;
    }
    if (options[option_index]->is_highlighted) {
        options[option_index]->move_right();
    } else {
        option_index = std::min(option_index + 1, static_cast<int>(options.size()) - 1);
    }
}

void SettingsBox::select_option() {
    if (options.empty()) {
        return;
    }
    options[option_index]->is_highlighted = !options[option_index]->is_highlighted;
    if (options[option_index]->needs_text_input()) {
        set_keyboard_visible(options[option_index]->is_highlighted, options[option_index]->text_input_value());
    }
    options[option_index]->confirm();
}

bool SettingsBox::editing_text() const {
    return !options.empty() && options[option_index]->is_highlighted && options[option_index]->needs_text_input();
}

// Any option capturing input (text, keybind, value list) owns the back key.
bool SettingsBox::editing() const {
    return !options.empty() && options[option_index]->is_highlighted;
}

void SettingsBox::select() {
    in_box = true;
}

std::optional<Screens> SettingsBox::pending_screen_change() {
    for (auto& opt : options) {
        auto* ao = dynamic_cast<AudioOffsetOptionBox*>(opt.get());
        if (ao && ao->wants_screen_change) {
            ao->wants_screen_change = false;
            return ao->pending_screen;
        }
    }
    return std::nullopt;
}

void SettingsBox::update(double current_time_ms, bool selected) {
    is_selected = selected;
    blue_arrow_fade->update(current_time_ms);
    blue_arrow_move->update(current_time_ms);
    move_anim->update(current_time_ms);

    if (move_anim->is_finished) {
        y = target_position;
    } else {
        y = start_position + move_anim->attribute * direction;
    }
    for (auto& opt : options) {
        opt->update(current_time_ms);
    }
}

void SettingsBox::draw_text() const {
    if (!t_box) {
        return;
    }
    float text_x = x + t_box->x[0] + (t_box->x2[0] / 2.0f) - (label->width / 2.0f);
    float text_y = y + t_box->y[0] + (t_box->y2[0] / 2.0f) - (label->height / 2.0f);

    if (is_selected) {
        label->draw({.x = text_x, .y = text_y});
    } else if (box_name == "exit") {
        label->draw({.color = Color::RED, .x = text_x, .y = text_y});
    } else {
        label->draw({.x = text_x, .y = text_y});
    }
}

void SettingsBox::draw() {
    tex.draw_texture(t_box, {.x = x, .y = y});
    if (is_selected) {
        tex.draw_texture(t_box_highlight, {.x = x, .y = y});
    }
    if (in_box && !options.empty()) {
        options[option_index]->draw();
        if (!options[option_index]->is_highlighted) {
            tex.draw_texture(
                t_blue_arrow,
                {.x = -static_cast<float>(blue_arrow_move->attribute), .fade = blue_arrow_fade->attribute, .index = 0});
            if (option_index != std::ssize(options) - 1) {
                tex.draw_texture(t_blue_arrow, {.mirror = Mirror::HORIZONTAL,
                                                .x = static_cast<float>(blue_arrow_move->attribute),
                                                .fade = blue_arrow_fade->attribute,
                                                .index = 1});
            }
        }
    }
    draw_text();
}
