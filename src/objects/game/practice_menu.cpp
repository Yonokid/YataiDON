#include "practice_menu.h"

#include "../../libs/global_data.h"

void PracticeMenu::init_textures() {
    fn_draw = fn_draw_dialog = fn_hit_test = {};
    if (!load("PracticeMenu", "practice_menu")) {
        return;
    }
    fn_draw = lua_object["draw"];
    fn_draw_dialog = lua_object["draw_dialog"];
    fn_hit_test = lua_object["hit_test"];
}

void PracticeMenu::unload() {
    fn_draw = fn_draw_dialog = fn_hit_test = {};
    lua_object = sol::table();
}

void PracticeMenu::open_menu() {
    open = true;
    index = ROW_COUNT - 1;
}

void PracticeMenu::close() {
    open = false;
    dialog = Dialog::NONE;
    editing_marks = false;
    jumping_marks = false;
}

void PracticeMenu::step(bool right) {
    if (dialog != Dialog::NONE) {
        dialog_sel = right ? 1 : 0;
        return;
    }
    index = ((index + (right ? 1 : -1)) % ROW_COUNT + ROW_COUNT) % ROW_COUNT;
}

void PracticeMenu::open_mark_edit() {
    editing_marks = true;
}

void PracticeMenu::close_mark_edit() {
    editing_marks = false;
}

void PracticeMenu::open_jump_mode() {
    jumping_marks = true;
}

void PracticeMenu::open_review_end() {
    open = true;
    open_dialog(Dialog::REVIEW_END, false);
}

void PracticeMenu::open_dialog(Dialog which, bool auto_on) {
    dialog = which;

    switch (which) {
    case Dialog::AUTO:
        dialog_sel = auto_on ? 0 : 1;
        break;
    case Dialog::RESTART:
        dialog_sel = 0;
        break;
    case Dialog::ANOTHER:
        dialog_sel = 0;
        break;
    case Dialog::END:
    case Dialog::REVIEW_END:
        dialog_sel = 0;
        break;
    default:
        dialog = Dialog::NONE;
        return;
    }
}

PracticeMenu::Action PracticeMenu::activate(bool auto_on) {
    switch (index) {
    case 0: // leave practice altogether
        open_dialog(Dialog::END, auto_on);
        return Action::NONE;
    case 1: // pick another song
        open_dialog(Dialog::ANOTHER, auto_on);
        return Action::NONE;
    case 2: // restart from the top
        open_dialog(Dialog::RESTART, auto_on);
        return Action::NONE;
    case 3: // auto play ON/OFF dialog
        open_dialog(Dialog::AUTO, auto_on);
        return Action::NONE;
    case 4: // move the scrobble cursor to the jump point
        return Action::JUMP_TO_MARK;
    case 5: // register the current bar as the jump point
        return Action::SET_MARK;
    case 6: // close the menu, stay paused
        open = false;
        return Action::NONE;
    }
    return Action::NONE;
}

PracticeMenu::Action PracticeMenu::confirm() {
    Dialog which = dialog;
    bool left = (dialog_sel == 0);
    dialog = Dialog::NONE;

    switch (which) {
    case Dialog::AUTO:
        open = false;
        return left ? Action::AUTO_ON : Action::AUTO_OFF;
    case Dialog::RESTART:
        return left ? Action::RESTART : Action::NONE;
    case Dialog::ANOTHER:
        return left ? Action::ANOTHER_SONG : Action::NONE;
    case Dialog::END:
        return left ? Action::END_GAME : Action::NONE;
    case Dialog::REVIEW_END:
        open = false;
        return left ? Action::END_GAME : Action::NONE;
    default:
        return Action::NONE;
    }
}

const char* PracticeMenu::dialog_name() const {
    switch (dialog) {
    case Dialog::AUTO:
        return "auto";
    case Dialog::RESTART:
        return "restart";
    case Dialog::ANOTHER:
        return "another";
    case Dialog::END:
        return "end";
    case Dialog::REVIEW_END:
        return "review_end";
    default:
        return nullptr;
    }
}

// Handle mouse/touch input for menu navigation and selection
PracticeMenu::Action PracticeMenu::handle_mouse_input(const Camera2D& camera, Vec2 mouse_pos, bool clicked,
                                                      bool auto_on) {
    if (!open) {
        return Action::NONE;
    }

    // Convert mouse position from screen coordinates to virtual coordinates
    Vec2 virtual_mouse = get_screen_to_world_2d(mouse_pos, camera);

    const char* name = dialog_name();
    sol::optional<std::string> dlg = name ? sol::optional<std::string>(name) : sol::nullopt;
    auto hit = call_r<sol::optional<int>>(fn_hit_test, "PracticeMenu:hit_test", virtual_mouse.x, virtual_mouse.y, dlg);
    if (!hit || !*hit) {
        return Action::NONE;
    }
    int i = **hit;

    if (dialog != Dialog::NONE) {
        if (!clicked) {
            return Action::NONE;
        }
        dialog_sel = i;
        return confirm();
    }

    index = i; // hover selects
    return clicked ? activate(auto_on) : Action::NONE;
}

void PracticeMenu::draw() {
    call(fn_draw, "PracticeMenu:draw", index);
}

void PracticeMenu::draw_dialog() {
    if (const char* name = dialog_name()) {
        call(fn_draw_dialog, "PracticeMenu:draw_dialog", std::string(name), dialog_sel);
    }
}
