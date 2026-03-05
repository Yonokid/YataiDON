#pragma once
#include "../../libs/texture.h"
#include "../../libs/audio.h"
#include "../../libs/screen.h"
#include "../objects/global/timer.h"
#include "../../libs/text.h"
#include "../objects/game/transition.h"
#include "../objects/song_select/ura_switch.h"
#include "../objects/song_select/dan_transition.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/indicator.h"
#include "../objects/game/song_info.h"
#include "../objects/song_select/player.h"
#include "../objects/song_select/search_box.h"
#include "../objects/song_select/diff_sort.h"
#include "../objects/song_select/file_navigator/navigator.h"
#include "../objects/song_select/file_navigator/color_utils.h"

class SongSelectScreen : public Screen {
private:
    MoveAnimation* background_move;
    FadeAnimation* diff_fade_out;
    FadeAnimation* text_fade_out;
    FadeAnimation* text_fade_in;
    FadeAnimation* background_fade_change;
    FadeAnimation* blue_arrow_fade;
    MoveAnimation* blue_arrow_move;

    SongSelectState state;

    std::optional<Transition> game_transition;

    GenreIndex genre_index;
    GenreIndex last_genre_index;

    int selected_difficulty = -3;

    ray::Shader shader;
    ray::Color color;
    SongNum* song_num;

    void select_song(SongBox* song);

    void handle_input();

    void handle_input_browsing();
    void handle_input_selecting();

    void draw_overlays();

    void draw_background();

public:
    SongSelectScreen() : Screen("song_select") {
    }

    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
