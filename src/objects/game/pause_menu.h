#pragma once

#include "../../libs/file/script.h"

class PauseMenu : public LuaScript {
public:
    enum class Action {
        NONE,
        RESUME,
        RESTART,
        QUIT
    };

    static constexpr int ROW_COUNT = 3;

    bool open = false;
    int index = 0;

    void init_textures();
    void unload();

    void open_menu(double now_ms);
    void close();
    void step(bool down);
    Action confirm() const;

    void draw(double now_ms);

private:
    double opened_ms = 0.0;
    sol::protected_function fn_draw;
};
