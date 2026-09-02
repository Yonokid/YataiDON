#include "practice_menu.h"
#include "../../libs/global_data.h"

void PracticeMenu::open_menu() {
    open = true;
    build_text();
    index = (int)menu_text.size() - 1;
}

void PracticeMenu::close() {
    open = false;
    dialog = Dialog::NONE;
}

void PracticeMenu::step(bool right) {
    if (dialog != Dialog::NONE) {
        dialog_sel = 1 - dialog_sel;
        return;
    }
    int n = (int)menu_text.size();
    index = ((index + (right ? 1 : -1)) % n + n) % n;
}

void PracticeMenu::build_text() {
    menu_text.clear();

    const std::string& lang = global_data.config->general.language;

    static const SC ROWS[] = {
        SC::PRACTICE_MENU_END_GAME,
        SC::PRACTICE_MENU_ANOTHER_SONG,
        SC::PRACTICE_MENU_RESTART,
        SC::PRACTICE_MENU_AUTO_PLAY,
        SC::PRACTICE_MENU_JUMP_TO_MARK,
        SC::PRACTICE_MENU_SET_MARK,
        SC::PRACTICE_MENU_CLOSE,
    };

    for (SC row : ROWS) {
        std::string label = tex.skin_config[row].text.at(lang);
        int chars = 0;
        for (unsigned char c : label)
            if ((c & 0xC0) != 0x80) chars++;
        const int base_fs = (int)(tex.skin_config[SC::SONG_BOX_NAME].font_size);
        int   fs    = base_fs;
        float avail = tex.skin_config[SC::PRACTICE_MENU_LABEL].width;
        float v     = 1.0f;
        if (chars > 1) {
            v = (avail / fs - 1.0f) / (chars - 1);
            if (v < 0.7f) {
                v  = 0.7f;
                fs = (int)(avail / (1.0f + (chars - 1) * 0.7f));
            } else if (v > 1.0f) {
                v = 1.0f;
            }
        }
        float outline = 5.0f * ((float)fs / (float)base_fs);
        menu_text.push_back(std::make_unique<OutlinedText>(label, fs,
            ray::WHITE, ray::BLACK, true, outline, 2.0f, v));
    }
}

void PracticeMenu::open_dialog(Dialog which, bool auto_on) {
    dialog = which;

    const std::string& lang = global_data.config->general.language;
    SC title = SC::PRACTICE_MENU_CONFIRM_END, left = SC::PRACTICE_MENU_YES, right = SC::PRACTICE_MENU_NO;
    switch (which) {
        case Dialog::AUTO:
            title = SC::PRACTICE_MENU_CONFIRM_AUTO;
            left  = SC::PRACTICE_MENU_AUTO_ON;
            right = SC::PRACTICE_MENU_AUTO_OFF;
            dialog_sel = auto_on ? 0 : 1;
            break;
        case Dialog::RESTART:
            title = SC::PRACTICE_MENU_CONFIRM_RESTART;
            dialog_sel = 0;
            break;
        case Dialog::ANOTHER:
            title = SC::PRACTICE_MENU_CONFIRM_ANOTHER;
            dialog_sel = 0;
            break;
        case Dialog::END:
            title = SC::PRACTICE_MENU_CONFIRM_END;
            dialog_sel = 0;
            break;
        default: return;
    }

    int fs = (int)(tex.skin_config[SC::SONG_BOX_NAME].font_size);
    dlg_title = std::make_unique<OutlinedText>(tex.skin_config[title].text.at(lang), fs, ray::WHITE, ray::BLACK, false);
    dlg_left  = std::make_unique<OutlinedText>(tex.skin_config[left].text.at(lang),  fs, ray::WHITE, ray::BLACK, false);
    dlg_right = std::make_unique<OutlinedText>(tex.skin_config[right].text.at(lang), fs, ray::WHITE, ray::BLACK, false);
}

PracticeMenu::Action PracticeMenu::activate(bool auto_on) {
    switch (index) {
        case 0:   // leave practice altogether
            open_dialog(Dialog::END, auto_on);
            return Action::NONE;
        case 1:   // pick another song
            open_dialog(Dialog::ANOTHER, auto_on);
            return Action::NONE;
        case 2:   // restart from the top
            open_dialog(Dialog::RESTART, auto_on);
            return Action::NONE;
        case 3:   // auto play ON/OFF dialog
            open_dialog(Dialog::AUTO, auto_on);
            return Action::NONE;
        case 4:   // move the scrobble cursor to the jump point
            return Action::JUMP_TO_MARK;
        case 5:   // register the current bar as the jump point
            return Action::SET_MARK;
        case 6:   // close the menu, stay paused
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
        default:
            return Action::NONE;
    }
}

void PracticeMenu::draw() const {
    int n = (int)menu_text.size();
    if (n == 0) return;

    float bar_w   = tex.skin_config[SC::PRACTICE_MENU_BAR].width;
    float bar_h   = tex.skin_config[SC::PRACTICE_MENU_BAR].height;
    float gap     = tex.skin_config[SC::PRACTICE_MENU_BAR].x;
    float panel_w = tex.skin_config[SC::PRACTICE_MENU_PANEL].width;
    float panel_h = tex.skin_config[SC::PRACTICE_MENU_PANEL].height;
    float panel_x = (tex.screen_width - panel_w) / 2.0f;
    float panel_y = tex.skin_config[SC::PRACTICE_MENU_PANEL].y;
    float pad     = (panel_w - n * bar_w - (n - 1) * gap) / 2.0f;

    tex.draw_texture(PRACTICE::MENU_PANEL, {.x = panel_x, .y = panel_y});

    for (int i = 0; i < n; i++) {
        float x = panel_x + pad + i * (bar_w + gap);
        float y = panel_y + (panel_h - bar_h) / 2.0f;
        bool selected = (i == index);

        tex.draw_texture(selected ? PRACTICE::MENU_BAR_SELECTED : PRACTICE::MENU_BAR,
                         {.x = x, .y = y});

        OutlinedText* text = menu_text[i].get();
        float tx = x + (bar_w - text->width) / 2.0f;
        float ty = y + tex.skin_config[SC::PRACTICE_MENU_LABEL].y;   // top-aligned like the arcade, not centred
        text->draw({.x = tx, .y = ty});
    }
}

void PracticeMenu::draw_dialog() const {
    float panel_w = tex.skin_config[SC::PRACTICE_MENU_PANEL].width;
    float panel_h = tex.skin_config[SC::PRACTICE_MENU_PANEL].height;
    float panel_x = (tex.screen_width - panel_w) / 2.0f;
    float panel_y = tex.skin_config[SC::PRACTICE_MENU_PANEL].y;

    tex.draw_texture(PRACTICE::MENU_PANEL, {.x = panel_x, .y = panel_y});

    // The auto dialog stacks a label chip and the toggle row below its
    // title, so the title sits high; the yes/no confirms sit lower.
    float title_y = (dialog == Dialog::AUTO) ? tex.skin_config[SC::PRACTICE_MENU_DIALOG_TITLE_AUTO].y
                                              : tex.skin_config[SC::PRACTICE_MENU_DIALOG_TITLE].y;
    if (dlg_title)
        dlg_title->draw({.x = panel_x + (panel_w - dlg_title->width) / 2.0f,
                         .y = panel_y + title_y});

    float btn_w = tex.skin_config[SC::PRACTICE_MENU_DIALOG_BUTTON].width;
    float btn_h = tex.skin_config[SC::PRACTICE_MENU_DIALOG_BUTTON].height;
    float left_x, right_x, row_y;

    if (dialog == Dialog::AUTO) {
        // Don chip above an ON | slider | OFF row, like the arcade's.
        float label_w = tex.skin_config[SC::PRACTICE_MENU_AUTO_LABEL].width;
        tex.draw_texture(PRACTICE::MENU_AUTO_LABEL,
                         {.x = panel_x + (panel_w - label_w) / 2.0f,
                          .y = panel_y + tex.skin_config[SC::PRACTICE_MENU_AUTO_LABEL].y});
        float tog_w = tex.skin_config[SC::PRACTICE_MENU_AUTO_TOGGLE].width;
        float gap   = tex.skin_config[SC::PRACTICE_MENU_AUTO_TOGGLE].x;
        float total = btn_w * 2 + tog_w + gap * 2;
        left_x  = panel_x + (panel_w - total) / 2.0f;
        right_x = left_x + btn_w + gap + tog_w + gap;
        row_y   = panel_y + tex.skin_config[SC::PRACTICE_MENU_AUTO_TOGGLE].y;
        // The red knob slides toward whichever side is picked.
        tex.draw_texture(PRACTICE::MENU_TOGGLE,
                         {.mirror = dialog_sel == 1 ? Mirror::HORIZONTAL : Mirror::NONE,
                          .x = left_x + btn_w + gap, .y = row_y + 6 * tex.screen_scale});
    } else {
        left_x  = panel_x + tex.skin_config[SC::PRACTICE_MENU_DIALOG_BUTTON].x;
        right_x = panel_x + panel_w - tex.skin_config[SC::PRACTICE_MENU_DIALOG_BUTTON].x - btn_w;
        row_y   = panel_y + tex.skin_config[SC::PRACTICE_MENU_DIALOG_BUTTON].y;
    }

    tex.draw_texture(dialog_sel == 0 ? PRACTICE::MENU_BUTTON_SELECTED : PRACTICE::MENU_BUTTON,
                     {.x = left_x, .y = row_y});
    tex.draw_texture(dialog_sel == 1 ? PRACTICE::MENU_BUTTON_SELECTED : PRACTICE::MENU_BUTTON,
                     {.x = right_x, .y = row_y});

    if (dlg_left)
        dlg_left->draw({.x = left_x + (btn_w - dlg_left->width) / 2.0f,
                        .y = row_y + (btn_h - dlg_left->height) / 2.0f});
    if (dlg_right)
        dlg_right->draw({.x = right_x + (btn_w - dlg_right->width) / 2.0f,
                         .y = row_y + (btn_h - dlg_right->height) / 2.0f});
    (void)panel_h;
}
