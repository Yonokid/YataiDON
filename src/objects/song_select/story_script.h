#pragma once

#include "../../libs/file/script.h"

class StoryScript : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_input;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;
    sol::protected_function fn_played;

    void bind();

public:
    enum Action {
        LEFT = 1,
        RIGHT = 2,
        DECIDE = 3
    };

    template<typename... Args>
    StoryScript(const std::string& class_name, const std::string& script_name, Args&&... args) {
        if (load(class_name, script_name, std::forward<Args>(args)...)) {
            bind();
        }
    }
    bool unavailable() const { return !fn_input.valid(); }
    void update(double current_ms);
    void input(Action action);
    void draw();
    bool is_done();
    std::vector<std::string> played();
};
