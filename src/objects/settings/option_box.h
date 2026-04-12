#pragma once
#include "../../libs/config.h"
#include "../../libs/texture.h"
#include "../../libs/animation.h"
#include "../../libs/text.h"
#include "config_ref.h"
#include <map>
#include <vector>
#include <string>

class BaseOptionBox {
protected:
    std::string     description_text;
    ConfigRef       config_ref;
    OutlinedText*   name_text;

    void draw_base() const;

public:
    bool is_highlighted;

    BaseOptionBox(const std::string& name, const std::string& description,
                  const std::string& path);
    virtual ~BaseOptionBox();

    virtual void confirm() {}

    virtual void update(double /*current_time*/) {}
    virtual void move_left()  {}
    virtual void move_right() {}
    virtual void draw() { draw_base(); }
};

class BoolOptionBox : public BaseOptionBox {
    bool          value;
    OutlinedText* on_text;
    OutlinedText* off_text;
public:
    BoolOptionBox(const std::string& name, const std::string& description,
                  const std::string& path,
                  const std::string& true_label, const std::string& false_label);
    ~BoolOptionBox() override;

    void confirm()    override;
    void move_left()  override;  // set false
    void move_right() override;  // set true
    void draw()       override;
};

class IntOptionBox : public BaseOptionBox {
    int           value;
    OutlinedText* value_text;
    FadeAnimation* flicker_fade;
    std::vector<std::pair<int,std::string>> value_list;
    int           value_index;

    void rebuild_text();
public:
    IntOptionBox(const std::string& name, const std::string& description,
                 const std::string& path,
                 const std::map<std::string,std::string>& values);
    ~IntOptionBox() override;

    void confirm()    override;
    void update(double current_time) override;
    void move_left()  override;
    void move_right() override;
    void draw()       override;
};


class StrOptionBox : public BaseOptionBox {
    std::string   value;
    std::string   input_string;   // used for free-form editing
    OutlinedText* value_text;
    FadeAnimation* flicker_fade;
    std::vector<std::pair<std::string,std::string>> value_list;
    int           value_index;

    void rebuild_text();
public:
    StrOptionBox(const std::string& name, const std::string& description,
                 const std::string& path,
                 const std::map<std::string,std::string>& values);
    ~StrOptionBox() override;

    void confirm()    override;
    void update(double current_time) override;
    void move_left()  override;
    void move_right() override;
    void draw()       override;
};

class KeybindOptionBox : public BaseOptionBox {
    std::vector<int> value;
    OutlinedText*    value_text;
    FadeAnimation*   flicker_fade;

    void rebuild_text();
public:
    KeybindOptionBox(const std::string& name, const std::string& description,
                     const std::string& path);
    ~KeybindOptionBox() override;

    void confirm()    override;
    void update(double current_time) override;
    void draw()       override;
};

class KeyBindControllerOptionBox : public BaseOptionBox {
    std::vector<int> value;
    OutlinedText*    value_text;
    FadeAnimation*   flicker_fade;

    void rebuild_text();
public:
    KeyBindControllerOptionBox(const std::string& name, const std::string& description,
                                const std::string& path);
    ~KeyBindControllerOptionBox() override;

    void confirm()    override;
    void update(double current_time) override;
    void draw()       override;
};

class FloatOptionBox : public BaseOptionBox {
    float         value;
    OutlinedText* value_text;
    FadeAnimation* flicker_fade;

    void rebuild_text();
public:
    FloatOptionBox(const std::string& name, const std::string& description,
                   const std::string& path);
    ~FloatOptionBox() override;

    void confirm()    override;
    void update(double current_time) override;
    void move_left()  override;
    void move_right() override;
    void draw()       override;
};
