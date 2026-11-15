#include "subdiff_picker.h"

#include <algorithm>
#include <array>

#include "../../libs/input/input.h"

static constexpr int LEVEL_SELECT_SECONDS = 10;

static constexpr std::array<int, 4> FIRST_LEVEL = {1, 2, 6, 10};
static constexpr std::array<int, 4> LEVEL_COUNT = {1, 4, 4, 4};

void SubdiffPicker::reset() {
    menu.emplace();
    chosen_course = 0;
    cursor = 0;
    choosing = false;
    decided = false;
}

int SubdiffPicker::hovered_course(SongSelectState state, const SongSelectPlayer& player) const {
    if (!menu || !menu->loaded() || state != SongSelectState::SONG_SELECTED || tex.options[SCO::AI_NO_SUPPORT_LEVELS]) {
        return -1;
    }
    const auto course = static_cast<int>(player.selected_difficulty);
    return course >= 0 && course < static_cast<int>(LEVEL_COUNT.size()) ? course : -1;
}

bool SubdiffPicker::handle_input() {
    if (!choosing) {
        return false;
    }
    if (decided) {
        clear_input_buffers();
        return true;
    }
    const int last = LEVEL_COUNT[chosen_course] - 1;
    if (is_l_kat_pressed(global_data.player_num)) {
        audio.play_player_sound("kat", global_data.player_num, VolumePreset::HITSOUND);
        cursor = std::max(cursor - 1, 0);
    } else if (is_r_kat_pressed(global_data.player_num)) {
        audio.play_player_sound("kat", global_data.player_num, VolumePreset::HITSOUND);
        cursor = std::min(cursor + 1, last);
    } else if (is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num)) {
        audio.play_player_sound("don", global_data.player_num, VolumePreset::HITSOUND);
        decided = true;
    }
    return true;
}

bool SubdiffPicker::open_if_decided(int course, const SongSelectPlayer& player, std::unique_ptr<Timer>& timer,
                                    double ms) {
    if (choosing || course < 0 || !player.difficulty_decided() || LEVEL_COUNT[course] <= 1) {
        return false;
    }
    chosen_course = course;
    choosing = true;
    cursor = LEVEL_COUNT[course] - 1;
    timer = std::make_unique<Timer>(LEVEL_SELECT_SECONDS, ms, [this] { decided = true; });
    return true;
}

int SubdiffPicker::subdiff() const {
    const bool simplified = choosing && cursor < LEVEL_COUNT[chosen_course] - 1;
    return simplified ? FIRST_LEVEL[chosen_course] + cursor : 0;
}

void SubdiffPicker::draw(double ms, int course, const SongBox* song, int player, int tension) {
    if (!menu) {
        return;
    }
    const bool branching = course >= 0 && song != nullptr && song->course_info(course).is_branching;
    menu->draw(ms, course, cursor, choosing, branching, player, tension);
}
