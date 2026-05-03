#include "option_box.h"
#include "../../libs/animation.h"

std::string getKeyString(int key_code);

static constexpr int   OPTION_FONT_SIZE = 30;
static constexpr float DESC_X           = 450.0f;
static constexpr float DESC_Y           = 270.0f;
static constexpr float DESC_FONT_SIZE   = 25.0f;

static std::unique_ptr<FadeAnimation> make_flicker() {
    auto fa = std::make_unique<FadeAnimation>(400.0, 0.0, true, false, 1.0, 0.0,
                                  std::nullopt, std::nullopt, 0.0);
    fa->start();
    return fa;
}

BaseOptionBox::BaseOptionBox(const std::string& name,
                             const std::string& description,
                             const std::string& path)
    : description_text(description)
    , config_ref(get_config_ref(path))
    , name_text(std::make_unique<OutlinedText>(name, OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4))
    , is_highlighted(false)
{}

void BaseOptionBox::draw_base() const {
    tex.draw_texture(BACKGROUND::OVERLAY, {.scale=0.70f});
    if (is_highlighted) {
        tex.draw_texture(BACKGROUND::TITLE_HIGHLIGHT);
    } else {
        tex.draw_texture(BACKGROUND::TITLE);
    }
    auto& title_obj = tex.textures[BACKGROUND::TITLE];
    float text_x = title_obj->x[0] + (title_obj->width  / 2.0f) - (name_text->width  / 2.0f);
    float text_y = title_obj->y[0] + (title_obj->height / 8.0f);
    name_text->draw({.x=text_x, .y=text_y});

    ray::Font font = font_manager.get_font(description_text, (int)DESC_FONT_SIZE);
    ray::DrawTextEx(font, description_text.c_str(),
                    {DESC_X, DESC_Y}, DESC_FONT_SIZE, 1, ray::BLACK);
}

BoolOptionBox::BoolOptionBox(const std::string& name,
                             const std::string& description,
                             const std::string& path,
                             const std::string& true_label,
                             const std::string& false_label)
    : BaseOptionBox(name, description, path)
    , value(config_ref.get_bool())
    , on_text(std::make_unique<OutlinedText>(true_label,  OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4))
    , off_text(std::make_unique<OutlinedText>(false_label, OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4))
{}

void BoolOptionBox::confirm() {
    config_ref.set_bool(value);
}

void BoolOptionBox::move_left()  { value = false; }
void BoolOptionBox::move_right() { value = true;  }

void BoolOptionBox::draw() {
    draw_base();

    auto& btn = tex.textures[OPTION::BUTTON_ON];

    if (!value) {
        tex.draw_texture(OPTION::BUTTON_ON,  {.index=0});
    } else {
        tex.draw_texture(OPTION::BUTTON_OFF, {.index=0});
    }
    float ox = btn->x[0] + (btn->width / 2.0f) - (off_text->width  / 2.0f);
    float oy = btn->y[0] + (btn->height / 2.0f) - (off_text->height / 2.0f);
    off_text->draw({.x=ox, .y=oy});

    if (value) {
        tex.draw_texture(OPTION::BUTTON_ON,  {.index=1});
    } else {
        tex.draw_texture(OPTION::BUTTON_OFF, {.index=1});
    }
    float nx = btn->x[1] + (btn->width / 2.0f) - (on_text->width  / 2.0f);
    float ny = btn->y[1] + (btn->height / 2.0f) - (on_text->height / 2.0f);
    on_text->draw({.x=nx, .y=ny});
}

static std::string int_display(int v) { return std::to_string(v); }

IntOptionBox::IntOptionBox(const std::string& name,
                           const std::string& description,
                           const std::string& path,
                           const std::map<std::string,std::string>& values)
    : BaseOptionBox(name, description, path)
    , value(config_ref.get_int())
    , value_text(nullptr)
    , flicker_fade(make_flicker())
    , value_index(0)
{
    if (!values.empty()) {
        for (auto& [k, v] : values) {
            value_list.emplace_back(std::stoi(k), v);
        }

        for (int i = 0; i < (int)value_list.size(); i++) {
            if (value_list[i].first == value) {
                value_index = i;
                break;
            }
        }
        value_text = std::make_unique<OutlinedText>(value_list[value_index].second,
                                      OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4);
    } else {
        value_text = std::make_unique<OutlinedText>(int_display(value),
                                      OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4);
    }
}

void IntOptionBox::rebuild_text() {
    std::string label = value_list.empty()
        ? int_display(value)
        : value_list[value_index].second;
    value_text = std::make_unique<OutlinedText>(label, OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4);
}

void IntOptionBox::confirm() {
    config_ref.set_int(value);
}

void IntOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
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
        value_index = std::min((int)value_list.size() - 1, value_index + 1);
        value = value_list[value_index].first;
    } else {
        value++;
    }
    rebuild_text();
}

void IntOptionBox::draw() {
    draw_base();

    tex.draw_texture(OPTION::BUTTON_OFF, {.index=2});
    if (is_highlighted) {
        tex.draw_texture(OPTION::BUTTON_ON,  {.fade=flicker_fade->attribute, .index=2});
    }
    auto& btn = tex.textures[OPTION::BUTTON_ON];
    float tx = btn->x[2] + (btn->width  / 2.0f) - (value_text->width  / 2.0f);
    float ty = btn->y[2] + (btn->height / 2.0f) - (value_text->height / 2.0f);
    value_text->draw({.x=tx, .y=ty});
}

StrOptionBox::StrOptionBox(const std::string& name,
                           const std::string& description,
                           const std::string& path,
                           const std::map<std::string,std::string>& values)
    : BaseOptionBox(name, description, path)
    , value(config_ref.get_str())
    , input_string(value)
    , value_text(nullptr)
    , flicker_fade(make_flicker())
    , value_index(0)
{
    if (!values.empty()) {
        for (auto& [k, v] : values) {
            value_list.emplace_back(k, v);
        }
        for (int i = 0; i < (int)value_list.size(); i++) {
            if (value_list[i].first == value) {
                value_index = i;
                break;
            }
        }
        value_text = std::make_unique<OutlinedText>(value_list[value_index].second,
                                      OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4);
    } else {
        value_text = std::make_unique<OutlinedText>(value, OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4);
    }
}

void StrOptionBox::rebuild_text() {
    std::string label = value_list.empty()
        ? input_string
        : value_list[value_index].second;
    value_text = std::make_unique<OutlinedText>(label, OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4);
}

void StrOptionBox::confirm() {
    config_ref.set_str(value);
}

void StrOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
    if (is_highlighted && value_list.empty()) {
        if (ray::IsKeyPressed(ray::KEY_BACKSPACE) && !input_string.empty()) {
            input_string.pop_back();
            rebuild_text();
        } else if (ray::IsKeyPressed(ray::KEY_ENTER)) {
            value = input_string;
            confirm();
            is_highlighted = false;
        }
        int key = ray::GetCharPressed();
        if (key > 0) {
            input_string += static_cast<char>(key);
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
        value_index = std::min((int)value_list.size() - 1, value_index + 1);
        value = value_list[value_index].first;
        rebuild_text();
    }
}

void StrOptionBox::draw() {
    draw_base();

    tex.draw_texture(OPTION::BUTTON_OFF, {.index=2});
    if (is_highlighted) {
        tex.draw_texture(OPTION::BUTTON_ON,  {.fade=flicker_fade->attribute, .index=2});
    }
    auto& btn = tex.textures[OPTION::BUTTON_ON];
    float tx = btn->x[2] + (btn->width  / 2.0f) - (value_text->width  / 2.0f);
    float ty = btn->y[2] + (btn->height / 2.0f) - (value_text->height / 2.0f);
    value_text->draw({.x=tx, .y=ty});
}

KeybindOptionBox::KeybindOptionBox(const std::string& name,
                                   const std::string& description,
                                   const std::string& path)
    : BaseOptionBox(name, description, path)
    , value_text(nullptr)
    , flicker_fade(make_flicker())
{
    if (config_ref.is_int()) {
        value = {config_ref.get_int()};
    } else {
        value = config_ref.get_vec();
    }
    rebuild_text();
}

void KeybindOptionBox::rebuild_text() {
    std::string label;
    for (int i = 0; i < (int)value.size(); i++) {
        try { label += getKeyString(value[i]); }
        catch (...) { label += std::to_string(value[i]); }
        if (i + 1 < (int)value.size()) label += ", ";
    }
    value_text = std::make_unique<OutlinedText>(label.empty() ? "none" : label,
                                  OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4);
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
        int key = ray::GetKeyPressed();
        if (key > 0) {
            value = {key};
            confirm();
            audio->play_sound("don", "sound");
            rebuild_text();
            is_highlighted = false;
        }
    }
}

void KeybindOptionBox::draw() {
    draw_base();

    tex.draw_texture(OPTION::BUTTON_OFF, {.index=2});
    if (is_highlighted) {
        tex.draw_texture(OPTION::BUTTON_ON,  {.fade=flicker_fade->attribute, .index=2});
    }
    auto& btn = tex.textures[OPTION::BUTTON_ON];
    float tx = btn->x[2] + (btn->width  / 2.0f) - (value_text->width  / 2.0f);
    float ty = btn->y[2] + (btn->height / 2.0f) - (value_text->height / 2.0f);
    value_text->draw({.x=tx, .y=ty});
}

KeyBindControllerOptionBox::KeyBindControllerOptionBox(const std::string& name,
                                                        const std::string& description,
                                                        const std::string& path)
    : BaseOptionBox(name, description, path)
    , value_text(nullptr)
    , flicker_fade(make_flicker())
{
    if (config_ref.is_int()) {
        value = {config_ref.get_int()};
    } else {
        value = config_ref.get_vec();
    }
    rebuild_text();
}

void KeyBindControllerOptionBox::rebuild_text() {
    std::string label;
    for (int i = 0; i < (int)value.size(); i++) {
        label += std::to_string(value[i]);
        if (i + 1 < (int)value.size()) label += ", ";
    }
    value_text = std::make_unique<OutlinedText>(label.empty() ? "none" : label,
                                  OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4);
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
    if (is_highlighted) {
        int btn = ray::GetGamepadButtonPressed();
        if (btn > 0) {
            value = {btn};
            confirm();
            audio->play_sound("don", "sound");
            rebuild_text();
            is_highlighted = false;
        }
    }
}

void KeyBindControllerOptionBox::draw() {
    draw_base();

    tex.draw_texture(OPTION::BUTTON_OFF, {.index=2});
    if (is_highlighted) {
        tex.draw_texture(OPTION::BUTTON_ON,  {.fade=flicker_fade->attribute, .index=2});
    }
    auto& b = tex.textures[OPTION::BUTTON_ON];
    float tx = b->x[2] + (b->width  / 2.0f) - (value_text->width  / 2.0f);
    float ty = b->y[2] + (b->height / 2.0f) - (value_text->height / 2.0f);
    value_text->draw({.x=tx, .y=ty});
}

FloatOptionBox::FloatOptionBox(const std::string& name,
                               const std::string& description,
                               const std::string& path)
    : BaseOptionBox(name, description, path)
    , value(config_ref.get_float())
    , value_text(nullptr)
    , flicker_fade(make_flicker())
{
    rebuild_text();
}

void FloatOptionBox::rebuild_text() {
    std::string label = std::to_string((int)(value * 100)) + "%";
    value_text = std::make_unique<OutlinedText>(label, OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4);
}

void FloatOptionBox::confirm() {
    config_ref.set_float(value);
}

void FloatOptionBox::update(double current_time) {
    flicker_fade->update(current_time);
}

void FloatOptionBox::move_left() {
    value = ((value * 100.0f) - 1.0f) / 100.0f;
    rebuild_text();
}

void FloatOptionBox::move_right() {
    value = ((value * 100.0f) + 1.0f) / 100.0f;
    rebuild_text();
}

void FloatOptionBox::draw() {
    draw_base();

    tex.draw_texture(OPTION::BUTTON_OFF, {.index=2});
    if (is_highlighted) {
        tex.draw_texture(OPTION::BUTTON_ON,  {.fade=flicker_fade->attribute, .index=2});
    }
    auto& btn = tex.textures[OPTION::BUTTON_ON];
    float tx = btn->x[2] + (btn->width  / 2.0f) - (value_text->width  / 2.0f);
    float ty = btn->y[2] + (btn->height / 2.0f) - (value_text->height / 2.0f);
    value_text->draw({.x=tx, .y=ty});
}

AudioOffsetOptionBox::AudioOffsetOptionBox(const std::string& name,
                                           const std::string& description,
                                           const std::string& path,
                                           Screens             calibrate_screen)
    : BaseOptionBox(name, description, path)
    , value(config_ref.get_int())
    , calibrate_screen(calibrate_screen)
    , offset_highlighted(true)
    , value_text(nullptr)
    , calibrate_text(std::make_unique<OutlinedText>("Calibrate", OPTION_FONT_SIZE, ray::WHITE, ray::BLACK, false, 4, -4))
    , flicker_fade(make_flicker())
{
    rebuild_text();
}

void AudioOffsetOptionBox::rebuild_text() {
    std::string label = (value >= 0 ? "+" : "") + std::to_string(value) + " ms";
    value_text = std::make_unique<OutlinedText>(label, OPTION_FONT_SIZE,
                                  ray::WHITE, ray::BLACK, false, 4, -4);
}

void AudioOffsetOptionBox::confirm() {
    if (is_highlighted) return;  // entering — act on exit only
    if (offset_highlighted) {
        config_ref.set_int(value);
    } else {
        wants_screen_change = true;
        pending_screen      = calibrate_screen;
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
        tex.draw_texture(OPTION::BUTTON_ON,  {.fade=flicker_fade->attribute, .index=0});
    } else {
        tex.draw_texture(OPTION::BUTTON_OFF, {.index=0});
    }
    {
        auto& btn = tex.textures[OPTION::BUTTON_ON];
        float tx = btn->x[0] + (btn->width  / 2.0f) - (value_text->width  / 2.0f);
        float ty = btn->y[0] + (btn->height / 2.0f) - (value_text->height / 2.0f);
        value_text->draw({.x=tx, .y=ty});
    }

    // Calibrate button (index 1)
    if (!offset_highlighted && is_highlighted) {
        tex.draw_texture(OPTION::BUTTON_ON,  {.fade=flicker_fade->attribute, .index=1});
    } else {
        tex.draw_texture(OPTION::BUTTON_OFF, {.index=1});
    }
    {
        auto& btn = tex.textures[OPTION::BUTTON_ON];
        float tx = btn->x[1] + (btn->width  / 2.0f) - (calibrate_text->width  / 2.0f);
        float ty = btn->y[1] + (btn->height / 2.0f) - (calibrate_text->height / 2.0f);
        calibrate_text->draw({.x=tx, .y=ty});
    }
}
