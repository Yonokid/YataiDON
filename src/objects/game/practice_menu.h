#pragma once

#include <array>

#include "../../libs/file/script.h"

// The practice pause menu. This keeps the state (selected row, dialog, selected button) and the
// actions; the skin's PracticeMenu (game/practice_menu.lua) draws it and hit-tests the pointer.
class PracticeMenu : public LuaScript {
public:
    enum class Dialog {
        NONE,
        AUTO,
        RESTART,
        ANOTHER,
        END,
        REVIEW_END
    };

    enum class Action {
        NONE,
        END_GAME,
        ANOTHER_SONG,
        RESTART,
        JUMP_TO_MARK,
        SET_MARK,
        AUTO_ON,
        AUTO_OFF
    };

    static constexpr int MARK_SLOTS = 5;
    static constexpr int ROW_COUNT = 7;

    bool open = false;
    int index = 0;
    Dialog dialog = Dialog::NONE;
    int dialog_sel = 0; // 0 = left option, 1 = right option

    bool editing_marks = false;
    bool jumping_marks = false;

    void init_textures(); // loads the skin script
    void unload();        // drops the Lua object before the Lua state can go away

    void open_menu();
    void close();

    void step(bool right);

    Action activate(bool auto_on);

    Action confirm();

    // Handle mouse/touch input for menu navigation and selection
    Action handle_mouse_input(const Camera2D& camera, Vec2 mouse_pos, bool clicked, bool auto_on);

    // Jump-point editor, entered from the SET_MARK menu row.
    void open_mark_edit();
    void close_mark_edit();

    // Free-roam jump-point navigation, entered from the JUMP_TO_MARK menu row.
    void open_jump_mode();

    void open_review_end();

    void draw();
    void draw_dialog();

private:
    void open_dialog(Dialog which, bool auto_on);
    const char* dialog_name() const;

    sol::protected_function fn_draw, fn_draw_dialog, fn_hit_test;
};
