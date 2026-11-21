#include "option_box.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "../../libs/gfx/camera_utils.h"
#include "../../libs/gfx/qr_texture.h"
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

static constexpr float DROPDOWN_ROW_BASE = 50.0f;
static constexpr float DROPDOWN_PAD_BASE = 20.0f;
static constexpr float DROPDOWN_BOTTOM_BASE = 585.0f;
static constexpr float DROPDOWN_MARGIN_BASE = 10.0f;
static constexpr float DROPDOWN_LINE_BASE = 3.0f;
static constexpr Color DROPDOWN_FILL{255, 248, 230, 240};
static constexpr Color DROPDOWN_LINE{109, 68, 24, 255};
static constexpr Color DROPDOWN_CURRENT{255, 160, 40, 255};

void ValueDropdown::show(const std::vector<std::string>& texts, int current, Rect button) {
    const float s = tex.screen_scale;
    const float pad = DROPDOWN_PAD_BASE * s;
    const float margin = DROPDOWN_MARGIN_BASE * s;
    labels.clear();
    float widest = button.width;
    for (const auto& text : texts) {
        labels.push_back(
            std::make_unique<OutlinedText>(text, option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4));
        widest = std::max(widest, labels.back()->width + (2.0f * pad));
    }
    row_height = DROPDOWN_ROW_BASE * s;
    const float top = button.y + button.height;
    const float rows = std::max(1.0f, std::floor(((DROPDOWN_BOTTOM_BASE * s) - top) / row_height));
    const float width = std::min(widest, tex.screen_width - (2.0f * margin));
    const float x =
        std::clamp(button.x + (button.width / 2.0f) - (width / 2.0f), margin, tex.screen_width - margin - width);
    area = {x, top, width, std::min(static_cast<float>(labels.size()), rows) * row_height};
    reveal(current);
    open = true;
}

float ValueDropdown::max_scroll() const {
    return std::max(0.0f, (static_cast<float>(labels.size()) * row_height) - area.height);
}

std::optional<int> ValueDropdown::pick(Vec2 world) const {
    if (world.x < area.x || world.x >= area.x + area.width || world.y < area.y || world.y >= area.y + area.height) {
        return std::nullopt;
    }
    const int row = static_cast<int>((world.y - area.y + scroll) / row_height);
    if (row < 0 || row >= std::ssize(labels)) {
        return std::nullopt;
    }
    return row;
}

void ValueDropdown::scroll_by(float dy) {
    scroll = std::clamp(scroll - dy, 0.0f, max_scroll());
}

void ValueDropdown::reveal(int index) {
    scroll =
        std::clamp((static_cast<float>(index) * row_height) - ((area.height - row_height) / 2.0f), 0.0f, max_scroll());
}

void ValueDropdown::draw(int current) const {
    if (!open || labels.empty()) {
        return;
    }
    const float s = tex.screen_scale;
    const float pad = DROPDOWN_PAD_BASE * s;
    draw_rectangle(area, DROPDOWN_FILL);
    const int sx = virtual_to_screen_x(area.x);
    const int sy = virtual_to_screen_y(area.y);
    begin_scissor_mode(sx, sy, virtual_to_screen_x(area.x + area.width) - sx,
                       virtual_to_screen_y(area.y + area.height) - sy);
    for (int i = static_cast<int>(scroll / row_height); i < std::ssize(labels); i++) {
        const float row_y = area.y + (static_cast<float>(i) * row_height) - scroll;
        if (row_y >= area.y + area.height) {
            break;
        }
        if (i == current) {
            draw_rectangle({area.x, row_y, area.width, row_height}, DROPDOWN_CURRENT);
        }
        OutlinedText& label = *labels[i];
        const float w = std::min(label.width, area.width - pad);
        label.draw({.x = area.x + (area.width / 2.0f) - (w / 2.0f),
                    .y = row_y + (row_height / 2.0f) - (label.height / 2.0f),
                    .x2 = w - label.width});
    }
    end_scissor_mode();
    draw_rectangle_lines_ex(area, DROPDOWN_LINE_BASE * s, DROPDOWN_LINE);
}

template<class List>
static std::vector<std::string> list_labels(const List& list) {
    std::vector<std::string> out;
    out.reserve(list.size());
    for (const auto& [key, label] : list) {
        out.push_back(label);
    }
    return out;
}

BaseOptionBox::BaseOptionBox(const std::string& name, const std::string& description, const std::string& path)
    : description_text(word_wrap(description, static_cast<int>(DESC_FONT_SIZE_BASE * tex.screen_scale), 1,
                                 DESC_MAX_WIDTH_BASE * tex.screen_scale)),
      config_ref(get_config_ref(path)),
      name_text(std::make_unique<OutlinedText>(name, option_font_size(), Color::WHITE, Color::BLACK, false, 4, -4)),
      t_overlay(tex.get_texture("background/overlay")), t_title(tex.get_texture("background/title")),
      t_title_highlight(tex.get_texture("background/title_highlight")),
      t_button_on(tex.get_texture("option/button_on")), t_button_off(tex.get_texture("option/button_off")) {}

OptionTap BaseOptionBox::tap(Vec2 world) {
    return texture_hit(t_button_on, 2, world) ? OptionTap::Select : OptionTap::None;
}

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

OptionTap BoolOptionBox::tap(Vec2 world) {
    for (const int i : {0, 1}) {
        if (texture_hit(t_button_on, i, world)) {
            value = i == 1;
            confirm();
            is_highlighted = false;
            return OptionTap::Handled;
        }
    }
    return OptionTap::None;
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

OptionTap IntOptionBox::tap(Vec2 world) {
    if (dropdown.open) {
        if (const auto row = dropdown.pick(world)) {
            value_index = *row;
            value = value_list[value_index].first;
            rebuild_text();
            confirm();
        }
        dropdown.open = false;
        is_highlighted = false;
        return OptionTap::Handled;
    }
    if (value_list.empty() || !texture_hit(t_button_on, 2, world)) {
        return BaseOptionBox::tap(world);
    }
    dropdown.show(list_labels(value_list), value_index, button_rect(2));
    is_highlighted = true;
    return OptionTap::Handled;
}

bool IntOptionBox::scroll(float dy) {
    if (dropdown.open) {
        dropdown.scroll_by(dy);
    }
    return dropdown.open;
}

void IntOptionBox::draw_overlay() {
    dropdown.draw(value_index);
}

void IntOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
    if (!is_highlighted) {
        dropdown.open = false;
    }
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
        dropdown.reveal(value_index);
    } else {
        value--;
    }
    rebuild_text();
}

void IntOptionBox::move_right() {
    if (!value_list.empty()) {
        value_index = std::min(static_cast<int>(value_list.size()) - 1, value_index + 1);
        value = value_list[value_index].first;
        dropdown.reveal(value_index);
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

OptionTap StrOptionBox::tap(Vec2 world) {
    if (dropdown.open) {
        if (const auto row = dropdown.pick(world)) {
            value_index = *row;
            value = value_list[value_index].first;
            rebuild_text();
            confirm();
        }
        dropdown.open = false;
        is_highlighted = false;
        return OptionTap::Handled;
    }
    if (value_list.empty() || !texture_hit(t_button_on, 2, world)) {
        return BaseOptionBox::tap(world);
    }
    dropdown.show(list_labels(value_list), value_index, button_rect(2));
    is_highlighted = true;
    return OptionTap::Handled;
}

bool StrOptionBox::scroll(float dy) {
    if (dropdown.open) {
        dropdown.scroll_by(dy);
    }
    return dropdown.open;
}

void StrOptionBox::draw_overlay() {
    dropdown.draw(value_index);
}

void StrOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
    if (!is_highlighted) {
        dropdown.open = false;
    }
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
        dropdown.reveal(value_index);
        rebuild_text();
    }
}

void StrOptionBox::move_right() {
    if (!value_list.empty()) {
        value_index = std::min(static_cast<int>(value_list.size()) - 1, value_index + 1);
        value = value_list[value_index].first;
        dropdown.reveal(value_index);
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

static constexpr float QR_SIZE_BASE = 200.0f;
static constexpr float QR_GAP_BASE = 20.0f;
AccountOptionBox::~AccountOptionBox() {
    if (qr_texture) {
        unload_texture(*qr_texture);
    }
}

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
    global_data.card.overridden[slot] = false;
}

// Both slots sharing an account would overwrite each other's data, so the other slot's account is skipped.
void AccountOptionBox::step(int direction) {
    const auto& network = global_data.config->network;
    const std::string& other = slot == 0 ? network.access_code_2 : network.access_code_1;
    for (int i = account_index + direction; i >= 0 && i < std::ssize(accounts); i += direction) {
        if (accounts[i] != other) {
            choose(i);
            return;
        }
    }
}

void AccountOptionBox::choose(int index) {
    account_index = index;
    value = accounts[index];
    code_revealed = false;
    chara.reset();
    if (qr_texture) {
        unload_texture(*qr_texture);
        qr_texture.reset();
    }
    dropdown.reveal(dropdown_row());
}

int AccountOptionBox::dropdown_row() const {
    return static_cast<int>(
        std::distance(dropdown_accounts.begin(), std::ranges::find(dropdown_accounts, account_index)));
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

Rect AccountOptionBox::code_box() const {
    const float s = tex.screen_scale;
    return {t_button_off->x[2] + (CODE_BOX_X_OFFSET_BASE * s), t_button_off->y[2] + (CODE_BOX_Y_OFFSET_BASE * s),
            t_button_off->x2[2] + (CODE_BOX_EXTRA_WIDTH_BASE * s), static_cast<float>(t_button_off->y2[2])};
}

OptionTap AccountOptionBox::tap(Vec2 world) {
    if (dropdown.open) {
        if (const auto row = dropdown.pick(world)) {
            choose(dropdown_accounts[*row]);
            is_highlighted = false;
            confirm();
        }
        dropdown.open = false;
        is_highlighted = false;
        return OptionTap::Handled;
    }
    if (texture_hit(t_button_on, 2, world)) {
        const auto& network = global_data.config->network;
        const std::string& other = slot == 0 ? network.access_code_2 : network.access_code_1;
        std::vector<std::string> names;
        dropdown_accounts.clear();
        int current = 0;
        for (int i = 0; i < std::ssize(accounts); i++) {
            if (accounts[i] == other) {
                continue;
            }
            const std::string name = accounts[i] == slot_data().player_id
                                         ? slot_data().username
                                         : scores_manager.get_player_data(accounts[i]).value_or(PlayerData{}).username;
            if (i == account_index) {
                current = static_cast<int>(names.size());
            }
            names.push_back(name.empty() ? accounts[i] : name);
            dropdown_accounts.push_back(i);
        }
        dropdown.show(names, current, button_rect(2));
        is_highlighted = true;
        return OptionTap::Handled;
    }
    const Rect box = code_box();
    if (world.x >= box.x && world.x < box.x + box.width && world.y >= box.y && world.y < box.y + box.height) {
        code_revealed = !code_revealed;
        return OptionTap::Handled;
    }
    return BaseOptionBox::tap(world);
}

bool AccountOptionBox::scroll(float dy) {
    if (dropdown.open) {
        dropdown.scroll_by(dy);
    }
    return dropdown.open;
}

void AccountOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
    if (!is_highlighted) {
        dropdown.open = false;
    }
    if (!drawn) {
        chara.reset();
        code_revealed = false;
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

    const auto [box_x, box_y, box_w, box_h] = code_box();
    tex.draw_texture(t_button_off, {.x = CODE_BOX_X_OFFSET_BASE * s,
                                    .y = CODE_BOX_Y_OFFSET_BASE * s,
                                    .x2 = CODE_BOX_EXTRA_WIDTH_BASE * s,
                                    .index = 2});
    const Vec2 mouse = get_screen_to_world_2d(mouse_position(), compute_camera2d(tex.screen_width, tex.screen_height));
    const bool shown =
        code_revealed || (mouse.x >= box_x && mouse.x < box_x + box_w && mouse.y >= box_y && mouse.y < box_y + box_h);
    OutlinedText& code = shown ? *code_text : *masked_text;
    qr_visible = shown && !value.empty();
    if (qr_visible) {
        if (!qr_texture) {
            qr_texture = make_qr_texture(access_code_login_url(value));
        }
        const float size = QR_SIZE_BASE * s;
        const float qx = std::min(box_x + box_w + (QR_GAP_BASE * s), tex.screen_width - size);
        qr_rect = {qx, box_y + (box_h / 2.0f) - (size / 2.0f), size, size};
    }
    code.draw({.x = box_x + (box_w / 2.0f) - (code.width / 2.0f), .y = box_y + (box_h / 2.0f) - (code.height / 2.0f)});
}

void AccountOptionBox::draw_overlay() {
    if (qr_visible && qr_texture) {
        qr_visible = false;
        const float src = static_cast<float>(qr_texture->width);
        draw_texture_pro(*qr_texture, {0, 0, src, src}, qr_rect, {0, 0}, 0.0f, Color::WHITE);
    }
    dropdown.draw(dropdown_row());
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
        editing_value = false;
        return;
    }
    if (editing_value) {
        config_ref.set_int(value);
        editing_value = false;
        return;
    }
    if (offset_highlighted) {
        is_highlighted = true;
        editing_value = true;
        return;
    }
    wants_screen_change = true;
    pending_screen = calibrate_screen;
}

OptionTap AudioOffsetOptionBox::tap(Vec2 world) {
    if (texture_hit(t_button_on, 0, world)) {
        if (editing_value) {
            return OptionTap::Select;
        }
        is_highlighted = true;
        offset_highlighted = true;
        editing_value = true;
        return OptionTap::Handled;
    }
    if (texture_hit(t_button_on, 1, world)) {
        is_highlighted = false;
        offset_highlighted = false;
        editing_value = false;
        confirm();
        return OptionTap::Handled;
    }
    return OptionTap::None;
}

void AudioOffsetOptionBox::move_left() {
    if (editing_value) {
        value -= 1;
        rebuild_text();
    } else {
        offset_highlighted = true;
    }
}

void AudioOffsetOptionBox::move_right() {
    if (editing_value) {
        value += 1;
        rebuild_text();
    } else {
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
        tex.draw_texture(t_button_on, {.fade = editing_value ? 1.1 : flicker_fade->attribute, .index = 0});
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
