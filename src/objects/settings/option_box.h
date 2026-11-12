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

public:
    virtual ~BaseOptionBox() = default;

    bool is_highlighted{false};

    BaseOptionBox(const std::string& name, const std::string& description, const std::string& path);

    virtual void confirm() {}
    virtual bool needs_text_input() const { return false; }
    virtual std::string text_input_value() const { return {}; }

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

    void rebuild_text();

public:
    IntOptionBox(const std::string& name, const std::string& description, const std::string& path,
                 const std::map<std::string, std::string>& values);

    void confirm() override;
    void update(double current_time) override;
    void move_left() override;
    void move_right() override;
    void draw() override;
};

class StrOptionBox : public BaseOptionBox {
    std::string value;
    std::string input_string; // used for free-form editing
    std::unique_ptr<OutlinedText> value_text;
    std::unique_ptr<FadeAnimation> flicker_fade;
    std::vector<std::pair<std::string, std::string>> value_list;
    int value_index{0};

    void rebuild_text();

public:
    StrOptionBox(const std::string& name, const std::string& description, const std::string& path,
                 const std::map<std::string, std::string>& values);

    void confirm() override;
    bool needs_text_input() const override;
    std::string text_input_value() const override { return input_string; }
    void update(double current_time) override;
    void move_left() override;
    void move_right() override;
    void draw() override;
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
    Rect qr_rect{};
    bool qr_visible{false};
    // Held only while this option is on screen, so it never goes stale and opening settings stays fast
    std::unique_ptr<Nameplate> nameplate;
    std::unique_ptr<Chara3D> chara;
    bool drawn{false};

    PlayerData& slot_data() const;
    void step(int direction);
    void load_preview();

public:
    AccountOptionBox(const std::string& name, const std::string& description, const std::string& path);
    ~AccountOptionBox() override;

    void confirm() override;
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
    std::unique_ptr<OutlinedText> value_text;
    std::unique_ptr<OutlinedText> calibrate_text;
    std::unique_ptr<FadeAnimation> flicker_fade;

    void rebuild_text();
};
