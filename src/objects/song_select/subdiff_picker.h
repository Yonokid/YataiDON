#pragma once

#include <memory>
#include <optional>

#include "../global/timer.h"
#include "ai_support_menu.h"
#include "file_navigator/box_song.h"
#include "player.h"

class SubdiffPicker {
    std::optional<AISupportMenu> menu;
    int chosen_course = 0;
    int cursor = 0;
    bool choosing = false;
    bool decided = false;

public:
    void reset();
    void clear() { menu.reset(); }
    AISupportMenu* support_menu() { return menu ? &*menu : nullptr; }
    int hovered_course(SongSelectState state, const SongSelectPlayer& player) const;
    bool handle_input();
    bool open_if_decided(int course, const SongSelectPlayer& player, std::unique_ptr<Timer>& timer, double ms);
    bool blocks_select() const { return choosing && !decided; }
    int subdiff() const;
    void draw(double ms, int course, const SongBox* song, int player, int tension);
};
