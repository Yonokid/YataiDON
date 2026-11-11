#include "option_box.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "../../libs/gfx/camera_utils.h"
#include "../../libs/input/ime_ui.h"
#include "../../libs/input/input.h"
#include "../../libs/math/animation.h"
#include "../song_select/file_navigator/text_layout.h"

static constexpr int OPTION_FONT_SIZE_BASE = 30;
static constexpr float DESC_X_BASE = 450.0f;
static constexpr float DESC_Y_BASE = 270.0f;
static constexpr float DESC_FONT_SIZE_BASE = 25.0f;
static constexpr float DESC_MAX_WIDTH_BASE = 790.0f;

// These are laid out for the 1280x720 base canvas; scale with the skin so
// higher-resolution skins don't drift out of alignment with the box art.
static int option_font_size() {
    return static_cast<int>(OPTION_FONT_SIZE_BASE * tex.screen_scale);
}

static std::unique_ptr<FadeAnimation> make_flicker() {
    auto fa = std::make_unique<FadeAnimation>(400.0, 0.0, true, false, 1.0, 0.0, std::nullopt, std::nullopt, 0.0);
    fa->start();
    return fa;
}

BaseOptionBox::BaseOptionBox(const std::string& name, const std::string& description, const std::string& path)
    : description_text(word_wrap(description, static_cast<int>(DESC_FONT_SIZE_BASE * tex.screen_scale), 1,
                                 DESC_MAX_WIDTH_BASE * tex.screen_scale)),
      config_ref(get_config_ref(path)),
      name_text(std::make_unique<OutlinedText>(name, option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4)),
      t_overlay(tex.get_texture("background/overlay")), t_title(tex.get_texture("background/title")),
      t_title_highlight(tex.get_texture("background/title_highlight")),
      t_button_on(tex.get_texture("option/button_on")), t_button_off(tex.get_texture("option/button_off")) {}

void BaseOptionBox::draw_base() const {
    tex.draw_texture(t_overlay, {.scale = 0.70f});
    if (is_highlighted) {
        tex.draw_texture(t_title_highlight);
    } else {
        tex.draw_texture(t_title);
    }
    float text_x = t_title->x[0] + (t_title->x2[0] / 2.0f) - (name_text->width / 2.0f);
    float text_y = t_title->y[0] + (t_title->y2[0] / 8.0f);
    name_text->draw({.x = text_x, .y = text_y});

    float desc_font_size = DESC_FONT_SIZE_BASE * tex.screen_scale;
    Font font = font_manager.get_font(description_text, static_cast<int>(desc_font_size));
    draw_text_ex(font, description_text.c_str(), {DESC_X_BASE * tex.screen_scale, DESC_Y_BASE * tex.screen_scale},
                 desc_font_size, 1, Color::BLACK);
}

BoolOptionBox::BoolOptionBox(const std::string& name, const std::string& description, const std::string& path,
                             const std::string& true_label, const std::string& false_label)
    : BaseOptionBox(name, description, path), value(config_ref.get_bool()),
      on_text(std::make_unique<OutlinedText>(true_label, option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4)),
      off_text(
          std::make_unique<OutlinedText>(false_label, option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4)) {}

void BoolOptionBox::confirm() {
    config_ref.set_bool(value);
}

// Nameplate options point at the active account, which AccountOptionBox can swap.
void BoolOptionBox::update(double /*current_time*/) {
    if (!is_highlighted) {
        value = config_ref.get_bool();
    }
}

void BoolOptionBox::move_left() {
    value = false;
}
void BoolOptionBox::move_right() {
    value = true;
}

void BoolOptionBox::draw() {
    draw_base();

    if (!value) {
        tex.draw_texture(t_button_on, {.index = 0});
    } else {
        tex.draw_texture(t_button_off, {.index = 0});
    }
    float ox = t_button_on->x[0] + (t_button_on->x2[0] / 2.0f) - (off_text->width / 2.0f);
    float oy = t_button_on->y[0] + (t_button_on->y2[0] / 2.0f) - (off_text->height / 2.0f);
    off_text->draw({.x = ox, .y = oy});

    if (value) {
        tex.draw_texture(t_button_on, {.index = 1});
    } else {
        tex.draw_texture(t_button_off, {.index = 1});
    }
    float nx = t_button_on->x[1] + (t_button_on->x2[1] / 2.0f) - (on_text->width / 2.0f);
    float ny = t_button_on->y[1] + (t_button_on->y2[1] / 2.0f) - (on_text->height / 2.0f);
    on_text->draw({.x = nx, .y = ny});
}

static std::string int_display(int v) {
    return std::to_string(v);
}

IntOptionBox::IntOptionBox(const std::string& name, const std::string& description, const std::string& path,
                           const std::map<std::string, std::string>& values)
    : BaseOptionBox(name, description, path), value(config_ref.get_int()), value_text(nullptr),
      flicker_fade(make_flicker()) {
    if (!values.empty()) {
        for (auto& [k, v] : values) {
            try {
                value_list.emplace_back(std::stoi(k), v);
            } catch (const std::exception&) {
                continue;
            }
        }

        std::ranges::sort(value_list, [](const auto& a, const auto& b) { return a.first < b.first; });

        if (!value_list.empty()) {
            bool matched = false;
            for (int i = 0; i < std::ssize(value_list); i++) {
                if (value_list[i].first == value) {
                    value_index = i;
                    matched = true;
                    break;
                }
            }
            if (!matched) {

                value_index = 0;
                value = value_list[0].first;
            }
            value_text = std::make_unique<OutlinedText>(value_list[value_index].second, option_font_size(),
                                                        Color::WHITE, Color::BLACK, false, 4, -4);
        } else {
            value_text = std::make_unique<OutlinedText>(int_display(value), option_font_size(), Color::WHITE,
                                                        Color::BLACK, false, 4, -4);
        }
    } else {
        value_text = std::make_unique<OutlinedText>(int_display(value), option_font_size(), Color::WHITE, Color::BLACK,
                                                    false, 4, -4);
    }
}

void IntOptionBox::rebuild_text() {
    std::string label = value_list.empty() ? int_display(value) : value_list[value_index].second;
    value_text = std::make_unique<OutlinedText>(label, option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4);
}

void IntOptionBox::confirm() {
    config_ref.set_int(value);
}

void IntOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
    // The engine can adjust a value after it is set (e.g. a display rejecting a refresh rate).
    if (!is_highlighted && config_ref.get_int() != value) {
        value = config_ref.get_int();
        for (int i = 0; i < std::ssize(value_list); i++) {
            if (value_list[i].first == value) {
                value_index = i;
            }
        }
        rebuild_text();
    }
}

void IntOptionBox::move_left() {
    if (!value_list.empty()) {
        value_index = std::max(0, value_index - 1);
        value = value_list[value_index].first;
    } else {
        value--;
    }
    rebuild_text();
}

void IntOptionBox::move_right() {
    if (!value_list.empty()) {
        value_index = std::min(static_cast<int>(value_list.size()) - 1, value_index + 1);
        value = value_list[value_index].first;
    } else {
        value++;
    }
    rebuild_text();
}

void IntOptionBox::draw() {
    draw_base();

    tex.draw_texture(t_button_off, {.index = 2});
    if (is_highlighted) {
        tex.draw_texture(t_button_on, {.fade = flicker_fade->attribute, .index = 2});
    }
    float tx = t_button_on->x[2] + (t_button_on->x2[2] / 2.0f) - (value_text->width / 2.0f);
    float ty = t_button_on->y[2] + (t_button_on->y2[2] / 2.0f) - (value_text->height / 2.0f);
    value_text->draw({.x = tx, .y = ty});
}

StrOptionBox::StrOptionBox(const std::string& name, const std::string& description, const std::string& path,
                           const std::map<std::string, std::string>& values)
    : BaseOptionBox(name, description, path), value(config_ref.get_str()), input_string(value), value_text(nullptr),
      flicker_fade(make_flicker()) {
    if (!values.empty()) {
        for (auto& [k, v] : values) {
            value_list.emplace_back(k, v);
        }
        bool matched = false;
        for (int i = 0; i < std::ssize(value_list); i++) {
            if (value_list[i].first == value) {
                value_index = i;
                matched = true;
                break;
            }
        }
        if (!matched) {
            value_index = 0;
            value = value_list[0].first;
        }
        value_text = std::make_unique<OutlinedText>(value_list[value_index].second, option_font_size(), Color::WHITE,
                                                    Color::BLACK, false, 4, -4);
    } else {
        value_text =
            std::make_unique<OutlinedText>(value, option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4);
    }
}

void StrOptionBox::rebuild_text() {
    std::string label = value_list.empty() ? input_string : value_list[value_index].second;
    value_text = std::make_unique<OutlinedText>(label, option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4);
}

void StrOptionBox::confirm() {
    config_ref.set_str(value);
}

bool StrOptionBox::needs_text_input() const {
    return value_list.empty();
}

void StrOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
    // Nameplate options point at the active account, which AccountOptionBox can swap.
    if (!is_highlighted && value_list.empty() && config_ref.get_str() != value) {
        value = input_string = config_ref.get_str();
        rebuild_text();
    }
    if (is_highlighted && value_list.empty()) {
        std::string before = input_string;
        TextEditAction action = poll_text_edit(input_string);
#if defined(PLATFORM_ANDROID) || defined(YATAIDON_PLATFORM_IOS)
        if (action == TextEditAction::None && (is_l_don_pressed() || is_r_don_pressed())) {
            action = TextEditAction::Confirm;
        }
#endif
        if (action == TextEditAction::Confirm) {
            value = input_string;
            confirm();
            is_highlighted = false;
            set_keyboard_visible(false);
        }
        if (action == TextEditAction::Cancel) {
            input_string = value;
            is_highlighted = false;
            set_keyboard_visible(false);
        }
        if (input_string != before) {
            rebuild_text();
        }
    }
}

void StrOptionBox::move_left() {
    if (!value_list.empty()) {
        value_index = std::max(0, value_index - 1);
        value = value_list[value_index].first;
        rebuild_text();
    }
}

void StrOptionBox::move_right() {
    if (!value_list.empty()) {
        value_index = std::min(static_cast<int>(value_list.size()) - 1, value_index + 1);
        value = value_list[value_index].first;
        rebuild_text();
    }
}

void StrOptionBox::draw() {
    draw_base();
    if (is_highlighted && value_list.empty()) {
        ime_ui_set_field({static_cast<float>(t_button_on->x[2]), static_cast<float>(t_button_on->y[2]),
                          static_cast<float>(t_button_on->x2[2]), static_cast<float>(t_button_on->y2[2])});
    }

    tex.draw_texture(t_button_off, {.index = 2});
    if (is_highlighted) {
        tex.draw_texture(t_button_on, {.fade = flicker_fade->attribute, .index = 2});
    }
    float tx = t_button_on->x[2] + (t_button_on->x2[2] / 2.0f) - (value_text->width / 2.0f);
    float ty = t_button_on->y[2] + (t_button_on->y2[2] / 2.0f) - (value_text->height / 2.0f);
    value_text->draw({.x = tx, .y = ty});
}

static constexpr float ACCOUNT_CHARA_X_BASE = 520.0f;
static constexpr float ACCOUNT_CHARA_Y_BASE = 565.0f;
static constexpr float ACCOUNT_PLATE_X_BASE = 705.0f;
static constexpr float ACCOUNT_PLATE_Y_BASE = 340.0f;
static constexpr float CODE_BOX_X_OFFSET_BASE = -94.0f; // from the value button
static constexpr float CODE_BOX_Y_OFFSET_BASE = 350.0f;
static constexpr float CODE_BOX_EXTRA_WIDTH_BASE = 188.0f; // fits a 20 digit code

AccountOptionBox::AccountOptionBox(const std::string& name, const std::string& description, const std::string& path)
    : BaseOptionBox(name, description, path), slot(path == "network/access_code_2" ? 1 : 0),
      value(config_ref.get_str()), accounts(scores_manager.get_player_ids()),
      masked_text(
          std::make_unique<OutlinedText>("********", option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4)),
      flicker_fade(make_flicker()) {
    auto current = std::ranges::find(accounts, value);
    if (current == accounts.end()) {
        current = accounts.insert(accounts.end(), value);
    }
    account_index = static_cast<int>(std::distance(accounts.begin(), current));
}

PlayerData& AccountOptionBox::slot_data() const {
    return slot == 0 ? scores_manager.player_1_data : scores_manager.player_2_data;
}

void AccountOptionBox::confirm() {
    if (is_highlighted || value == config_ref.get_str()) {
        return; // entering, or nothing picked
    }
    PlayerData& data = slot_data();
    scores_manager.save_player_data(data);
    config_ref.set_str(value);
    (slot == 0 ? scores_manager.player_1 : scores_manager.player_2) = value;
    data = scores_manager.get_player_data(value).value_or(PlayerData{});
    data.player_id = value;
    // A pick made during a card session outlives it; title and save_config would otherwise undo it.
    global_data.card_override[slot] = false;
}

// Both slots sharing an account would overwrite each other's data, so the other slot's account is skipped.
void AccountOptionBox::step(int direction) {
    const auto& network = global_data.config->network;
    const std::string& other = slot == 0 ? network.access_code_2 : network.access_code_1;
    for (int i = account_index + direction; i >= 0 && i < std::ssize(accounts); i += direction) {
        if (accounts[i] != other) {
            account_index = i;
            value = accounts[i];
            chara.reset();
            return;
        }
    }
}

void AccountOptionBox::move_left() {
    step(-1);
}

void AccountOptionBox::move_right() {
    step(1);
}

void AccountOptionBox::load_preview() {
    // The active account may hold nameplate edits that aren't saved until leaving settings.
    const PlayerData pd =
        value == slot_data().player_id ? slot_data() : scores_manager.get_player_data(value).value_or(PlayerData{});
    value_text =
        std::make_unique<OutlinedText>(pd.username, option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4);
    code_text = std::make_unique<OutlinedText>(value, option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4);
    nameplate =
        std::make_unique<Nameplate>(pd.username, pd.title, PlayerNum::ALL, pd.dan, pd.gold, pd.rainbow, pd.title_bg);
    chara = make_chara_from_player_data(&pd, slot == 1);
    chara->set_don_colors(pd.chara_color_1, pd.chara_color_2, pd.chara_color_3);
    chara->apply_face(pd.chara_face_index);
    chara->set_anim(AnimIndex::DON_ENTRY_LOOP);
}

void AccountOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
    if (!drawn) {
        chara.reset();
    }
    drawn = false;
    if (chara) {
        chara->update(current_time);
        nameplate->update(current_time);
    }
}

void AccountOptionBox::draw() {
    draw_base();
    if (!chara) {
        load_preview();
    }
    drawn = true;
    const float s = tex.screen_scale;

    tex.draw_texture(t_button_off, {.index = 2});
    if (is_highlighted) {
        tex.draw_texture(t_button_on, {.fade = flicker_fade->attribute, .index = 2});
    }
    const float value_width = std::min(value_text->width, t_button_on->x2[2] * 0.9f);
    float tx = t_button_on->x[2] + (t_button_on->x2[2] / 2.0f) - (value_width / 2.0f);
    float ty = t_button_on->y[2] + (t_button_on->y2[2] / 2.0f) - (value_text->height / 2.0f);
    value_text->draw({.x = tx, .y = ty, .x2 = value_width - value_text->width});

    chara->draw(ACCOUNT_CHARA_X_BASE * s, ACCOUNT_CHARA_Y_BASE * s);
    nameplate->draw(ACCOUNT_PLATE_X_BASE * s, ACCOUNT_PLATE_Y_BASE * s);

    const float box_x = t_button_off->x[2] + (CODE_BOX_X_OFFSET_BASE * s);
    const float box_y = t_button_off->y[2] + (CODE_BOX_Y_OFFSET_BASE * s);
    const float box_w = t_button_off->x2[2] + (CODE_BOX_EXTRA_WIDTH_BASE * s);
    const float box_h = t_button_off->y2[2];
    tex.draw_texture(t_button_off, {.x = CODE_BOX_X_OFFSET_BASE * s,
                                    .y = CODE_BOX_Y_OFFSET_BASE * s,
                                    .x2 = CODE_BOX_EXTRA_WIDTH_BASE * s,
                                    .index = 2});
    const Vec2 mouse = get_screen_to_world_2d(mouse_position(), compute_camera2d(tex.screen_width, tex.screen_height));
    const bool hovered = mouse.x >= box_x && mouse.x < box_x + box_w && mouse.y >= box_y && mouse.y < box_y + box_h;
    OutlinedText& code = hovered ? *code_text : *masked_text;
    code.draw({.x = box_x + (box_w / 2.0f) - (code.width / 2.0f), .y = box_y + (box_h / 2.0f) - (code.height / 2.0f)});
}

KeybindOptionBox::KeybindOptionBox(const std::string& name, const std::string& description, const std::string& path)
    : BaseOptionBox(name, description, path), value_text(nullptr), flicker_fade(make_flicker()) {
    if (config_ref.is_int()) {
        value = {config_ref.get_int()};
    } else {
        value = config_ref.get_vec();
    }
    rebuild_text();
}

void KeybindOptionBox::rebuild_text() {
    std::string label;
    for (int i = 0; i < std::ssize(value); i++) {
        try {
            label += getKeyString(value[i]);
        } catch (...) {
            label += std::to_string(value[i]);
        }
        if (i + 1 < std::ssize(value)) {
            label += ", ";
        }
    }
    value_text = std::make_unique<OutlinedText>(label.empty() ? "none" : label, option_font_size(), Color::WHITE,
                                                Color::BLACK, false, 4, -4);
}

void KeybindOptionBox::confirm() {
    if (config_ref.is_int()) {
        config_ref.set_int(value.empty() ? 0 : value[0]);
    } else {
        config_ref.set_vec(value);
    }
}

void KeybindOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
    if (is_highlighted) {
        int key = key_pressed_next();
        if (key > 0) {
            value = {key};
            confirm();
            audio.play_sound("don", VolumePreset::HITSOUND);
            rebuild_text();
            is_highlighted = false;
        }
    }
}

void KeybindOptionBox::draw() {
    draw_base();

    tex.draw_texture(t_button_off, {.index = 2});
    if (is_highlighted) {
        tex.draw_texture(t_button_on, {.fade = flicker_fade->attribute, .index = 2});
    }
    float tx = t_button_on->x[2] + (t_button_on->x2[2] / 2.0f) - (value_text->width / 2.0f);
    float ty = t_button_on->y[2] + (t_button_on->y2[2] / 2.0f) - (value_text->height / 2.0f);
    value_text->draw({.x = tx, .y = ty});
}

KeyBindControllerOptionBox::KeyBindControllerOptionBox(const std::string& name, const std::string& description,
                                                       const std::string& path)
    : BaseOptionBox(name, description, path), value_text(nullptr), flicker_fade(make_flicker()) {
    if (config_ref.is_int()) {
        value = {config_ref.get_int()};
    } else {
        value = config_ref.get_vec();
    }
    rebuild_text();
}

void KeyBindControllerOptionBox::rebuild_text() {
    std::string label;
    for (int i = 0; i < std::ssize(value); i++) {
        label += std::to_string(value[i]);
        if (i + 1 < std::ssize(value)) {
            label += ", ";
        }
    }
    value_text = std::make_unique<OutlinedText>(label.empty() ? "none" : label, option_font_size(), Color::WHITE,
                                                Color::BLACK, false, 4, -4);
}

void KeyBindControllerOptionBox::confirm() {
    if (config_ref.is_int()) {
        config_ref.set_int(value.empty() ? 0 : value[0]);
    } else {
        config_ref.set_vec(value);
    }
}

void KeyBindControllerOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
    if (!is_highlighted) {
        capture_armed = false;
        return;
    }
    if (!capture_armed) {
        // Drop the press that opened this box, plus anything buffered
        // before it, so capture starts clean.
        take_gamepad_button_pressed();
        capture_armed = true;
        return;
    }
    {
        int btn = take_gamepad_button_pressed();
        if (btn >= 0) {
            value = {btn};
            confirm();
            audio.play_sound("don", VolumePreset::HITSOUND);
            rebuild_text();
            is_highlighted = false;
        }
    }
}

void KeyBindControllerOptionBox::draw() {
    draw_base();

    tex.draw_texture(t_button_off, {.index = 2});
    if (is_highlighted) {
        tex.draw_texture(t_button_on, {.fade = flicker_fade->attribute, .index = 2});
    }
    float tx = t_button_on->x[2] + (t_button_on->x2[2] / 2.0f) - (value_text->width / 2.0f);
    float ty = t_button_on->y[2] + (t_button_on->y2[2] / 2.0f) - (value_text->height / 2.0f);
    value_text->draw({.x = tx, .y = ty});
}

FloatOptionBox::FloatOptionBox(const std::string& name, const std::string& description, const std::string& path)
    : BaseOptionBox(name, description, path), value(config_ref.get_float()), value_text(nullptr),
      flicker_fade(make_flicker()) {
    rebuild_text();
}

void FloatOptionBox::rebuild_text() {
    std::string label = std::to_string(static_cast<int>(value * 100)) + "%";
    value_text = std::make_unique<OutlinedText>(label, option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4);
}

void FloatOptionBox::confirm() {
    config_ref.set_float(value);
}

void FloatOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
}

void FloatOptionBox::move_left() {
    value = std::round((value * 100.0f) - 1.0f) / 100.0f;
    rebuild_text();
}

void FloatOptionBox::move_right() {
    value = std::round((value * 100.0f) + 1.0f) / 100.0f;
    rebuild_text();
}

void FloatOptionBox::draw() {
    draw_base();

    tex.draw_texture(t_button_off, {.index = 2});
    if (is_highlighted) {
        tex.draw_texture(t_button_on, {.fade = flicker_fade->attribute, .index = 2});
    }
    float tx = t_button_on->x[2] + (t_button_on->x2[2] / 2.0f) - (value_text->width / 2.0f);
    float ty = t_button_on->y[2] + (t_button_on->y2[2] / 2.0f) - (value_text->height / 2.0f);
    value_text->draw({.x = tx, .y = ty});
}

AudioOffsetOptionBox::AudioOffsetOptionBox(const std::string& name, const std::string& description,
                                           const std::string& path, Screens calibrate_screen)
    : BaseOptionBox(name, description, path), value(config_ref.get_int()), calibrate_screen(calibrate_screen),
      value_text(nullptr), calibrate_text(std::make_unique<OutlinedText>("Calibrate", option_font_size(), Color::WHITE,
                                                                         Color::BLACK, false, 4, -4)),
      flicker_fade(make_flicker()) {
    rebuild_text();
}

void AudioOffsetOptionBox::rebuild_text() {
    std::string label = (value >= 0 ? "+" : "") + std::to_string(value) + " ms";
    value_text = std::make_unique<OutlinedText>(label, option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4);
}

void AudioOffsetOptionBox::confirm() {
    if (is_highlighted) {
        return; // entering — act on exit only
    }
    if (offset_highlighted) {
        config_ref.set_int(value);
    } else {
        wants_screen_change = true;
        pending_screen = calibrate_screen;
    }
}

void AudioOffsetOptionBox::move_left() {
    if (offset_highlighted) {
        value -= 1;
        rebuild_text();
    } else {
        offset_highlighted = true;
    }
}

void AudioOffsetOptionBox::move_right() {
    if (offset_highlighted) {
        offset_highlighted = false;
    }
}

void AudioOffsetOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
}

void AudioOffsetOptionBox::draw() {
    draw_base();

    // Offset value button (index 0)
    if (offset_highlighted && is_highlighted) {
        tex.draw_texture(t_button_on, {.fade = flicker_fade->attribute, .index = 0});
    } else {
        tex.draw_texture(t_button_off, {.index = 0});
    }
    {
        float tx = t_button_on->x[0] + (t_button_on->x2[0] / 2.0f) - (value_text->width / 2.0f);
        float ty = t_button_on->y[0] + (t_button_on->y2[0] / 2.0f) - (value_text->height / 2.0f);
        value_text->draw({.x = tx, .y = ty});
    }

    // Calibrate button (index 1)
    if (!offset_highlighted && is_highlighted) {
        tex.draw_texture(t_button_on, {.fade = flicker_fade->attribute, .index = 1});
    } else {
        tex.draw_texture(t_button_off, {.index = 1});
    }
    {
        float tx = t_button_on->x[1] + (t_button_on->x2[1] / 2.0f) - (calibrate_text->width / 2.0f);
        float ty = t_button_on->y[1] + (t_button_on->y2[1] / 2.0f) - (calibrate_text->height / 2.0f);
        calibrate_text->draw({.x = tx, .y = ty});
    }
}
