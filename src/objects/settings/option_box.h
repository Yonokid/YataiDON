#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../../libs/gfx/text.h"
#include "../../libs/screen.h"
#include "../global/chara_3d.h"
#include "../global/nameplate.h"
#include "config_ref.h"

// Hit test on texture slot `index`, grown by `pad` on every side.
inline bool texture_hit(const TextureObject* t, int index, Vec2 p, float pad = 0.0f) {
    return t && index < std::ssize(t->x) && p.x >= t->x[index] - pad && p.x < t->x[index] + t->x2[index] + pad &&
           p.y >= t->y[index] - pad && p.y < t->y[index] + t->y2[index] + pad;
}

enum class OptionTap {
    None,
    Handled,
    Select, // toggle editing, like a confirm press
};

// Touch picker opened under an option's value button; drawn in the overlay pass.
class ValueDropdown {
    std::vector<std::unique_ptr<OutlinedText>> labels;
    Rect area;
    float row_height{0.0f};
    float scroll{0.0f};

    float max_scroll() const;

public:
    bool open{false};

    void show(const std::vector<std::string>& texts, int current, Rect button);
    // Row under the point, or nullopt outside the list.
    std::optional<int> pick(Vec2 world) const;
    void scroll_by(float dy);
    void reveal(int index);
    void draw(int current) const;
};

class BaseOptionBox {
protected:
    std::string description_text;
    ConfigRef config_ref;
    std::unique_ptr<OutlinedText> name_text;

    TextureObject* t_overlay = nullptr;
    TextureObject* t_title = nullptr;
    TextureObject* t_title_highlight = nullptr;
    TextureObject* t_button_on = nullptr;
    TextureObject* t_button_off = nullptr;

    void draw_base() const;
    Rect button_rect(int index) const {
        return {static_cast<float>(t_button_on->x[index]), static_cast<float>(t_button_on->y[index]),
                static_cast<float>(t_button_on->x2[index]), static_cast<float>(t_button_on->y2[index])};
    }

public:
    virtual ~BaseOptionBox() = default;

    bool is_highlighted{false};

    BaseOptionBox(const std::string& name, const std::string& description, const std::string& path);

    virtual void confirm() {}
    virtual bool needs_text_input() const { return false; }
    virtual std::string text_input_value() const { return {}; }

    virtual OptionTap tap(Vec2 world);
    // Touch drag while editing; true when the option used it (a dropdown scrolling).
    virtual bool scroll(float /*dy*/) { return false; }
    // While editing, taps left of this step the value down and right of it step it up.
    virtual float value_center_x() const { return t_button_on->x[2] + (t_button_on->x2[2] / 2.0f); }

    virtual void update(double /*current_time*/) {}
    virtual void move_left() {}
    virtual void move_right() {}
    virtual void draw() { draw_base(); }
    virtual void draw_overlay() {}
};

class BoolOptionBox : public BaseOptionBox {
    bool value;
    std::unique_ptr<OutlinedText> on_text;
    std::unique_ptr<OutlinedText> off_text;

public:
    BoolOptionBox(const std::string& name, const std::string& description, const std::string& path,
                  const std::string& true_label, const std::string& false_label);

    void confirm() override;
    OptionTap tap(Vec2 world) override;
    void move_left() override;  // set false
    void move_right() override; // set true
    void update(double current_time) override;
    void draw() override;
};

class IntOptionBox : public BaseOptionBox {
    int value;
    std::unique_ptr<OutlinedText> value_text;
    std::unique_ptr<FadeAnimation> flicker_fade;
    std::vector<std::pair<int, std::string>> value_list;
    int value_index{0};
    ValueDropdown dropdown;

    void rebuild_text();

public:
    IntOptionBox(const std::string& name, const std::string& description, const std::string& path,
                 const std::map<std::string, std::string>& values);

    void confirm() override;
    OptionTap tap(Vec2 world) override;
    bool scroll(float dy) override;
    void update(double current_time) override;
    void move_left() override;
    void move_right() override;
    void draw() override;
    void draw_overlay() override;
};

class StrOptionBox : public BaseOptionBox {
    std::string value;
    std::string input_string; // used for free-form editing
    std::unique_ptr<OutlinedText> value_text;
    std::unique_ptr<FadeAnimation> flicker_fade;
    std::vector<std::pair<std::string, std::string>> value_list;
    int value_index{0};
    ValueDropdown dropdown;

    void rebuild_text();

public:
    StrOptionBox(const std::string& name, const std::string& description, const std::string& path,
                 const std::map<std::string, std::string>& values);

    void confirm() override;
    OptionTap tap(Vec2 world) override;
    bool scroll(float dy) override;
    bool needs_text_input() const override;
    std::string text_input_value() const override { return input_string; }
    void update(double current_time) override;
    void move_left() override;
    void move_right() override;
    void draw() override;
    void draw_overlay() override;
};

// Picks which saved account (scores.db player) an access code slot logs in as.
class AccountOptionBox : public BaseOptionBox {
    int slot;
    std::string value;
    std::vector<std::string> accounts;
    int account_index{0};
    std::unique_ptr<OutlinedText> value_text;
    std::unique_ptr<OutlinedText> code_text;
    std::unique_ptr<OutlinedText> masked_text;
    std::unique_ptr<FadeAnimation> flicker_fade;
    std::optional<Texture> qr_texture;
    Rect qr_rect;
    bool qr_visible{false};
    bool code_revealed{false};
    ValueDropdown dropdown;
    std::vector<int> dropdown_accounts; // dropdown row -> accounts index
    // Held only while this option is on screen, so it never goes stale and opening settings stays fast
    std::unique_ptr<Nameplate> nameplate;
    std::unique_ptr<Chara3D> chara;
    bool drawn{false};

    PlayerData& slot_data() const;
    void step(int direction);
    void choose(int index);
    int dropdown_row() const;
    void load_preview();
    Rect code_box() const;

public:
    AccountOptionBox(const std::string& name, const std::string& description, const std::string& path);
    ~AccountOptionBox() override;

    void confirm() override;
    OptionTap tap(Vec2 world) override;
    bool scroll(float dy) override;
    void update(double current_time) override;
    void move_left() override;
    void move_right() override;
    void draw() override;
    void draw_overlay() override;
};

class KeybindOptionBox : public BaseOptionBox {
    std::vector<int> value;
    std::unique_ptr<OutlinedText> value_text;
    std::unique_ptr<FadeAnimation> flicker_fade;

    void rebuild_text();

public:
    KeybindOptionBox(const std::string& name, const std::string& description, const std::string& path);

    void confirm() override;
    void update(double current_time) override;
    void draw() override;
};

class KeyBindControllerOptionBox : public BaseOptionBox {
    std::vector<int> value;
    std::unique_ptr<OutlinedText> value_text;
    std::unique_ptr<FadeAnimation> flicker_fade;
    // Capture starts one frame after highlighting, so the press that opened
    // the box isn't immediately bound to it.
    bool capture_armed = false;

    void rebuild_text();

public:
    KeyBindControllerOptionBox(const std::string& name, const std::string& description, const std::string& path);

    void confirm() override;
    void update(double current_time) override;
    void draw() override;
};

class FloatOptionBox : public BaseOptionBox {
    float value;
    std::unique_ptr<OutlinedText> value_text;
    std::unique_ptr<FadeAnimation> flicker_fade;

    void rebuild_text();

public:
    FloatOptionBox(const std::string& name, const std::string& description, const std::string& path);

    void confirm() override;
    void update(double current_time) override;
    void move_left() override;
    void move_right() override;
    void draw() override;
};

class AudioOffsetOptionBox : public BaseOptionBox {
public:
    AudioOffsetOptionBox(const std::string& name, const std::string& description, const std::string& path,
                         Screens calibrate_screen);

    void confirm() override;
    OptionTap tap(Vec2 world) override;
    float value_center_x() const override { return t_button_on->x[0] + (t_button_on->x2[0] / 2.0f); }
    void move_left() override;
    void move_right() override;
    void draw() override;
    void update(double current_time) override;

    bool wants_screen_change = false;
    Screens pending_screen = Screens::INPUT_CALI;

private:
    int value;
    Screens calibrate_screen;
    bool offset_highlighted{true}; // true = on offset button, false = on calibrate button
    // Highlighted first picks a button; confirming the offset then edits its value until the next confirm.
    bool editing_value{false};
    std::unique_ptr<OutlinedText> value_text;
    std::unique_ptr<OutlinedText> calibrate_text;
    std::unique_ptr<FadeAnimation> flicker_fade;

    void rebuild_text();
};
