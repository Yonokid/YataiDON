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
protected:
    FadeAnimation* diff_fade_out;
    FadeAnimation* text_fade_in;
    FadeAnimation* blue_arrow_fade;
    MoveAnimation* blue_arrow_move;

    SongSelectState state;

    std::optional<Transition> game_transition;
    std::optional<DanTransition> dan_transition;
    CoinOverlay coin_overlay;
    AllNetIcon allnet_indicator;
    std::unique_ptr<Timer> select_timer;
    std::unique_ptr<Timer> diff_select_timer;
    std::unique_ptr<Indicator> indicator;
    Statistics cached_stats;

    ray::Shader shader;
    ray::Color color;
    std::unique_ptr<SongNum> song_num;

    std::unique_ptr<SongSelectPlayer> player;

    std::optional<DiffSortSelect> diff_sort_selector;
    std::pair<int,int> last_diff_sort = {-1, -1};

    std::optional<SearchBox> search_box;

    virtual void select_song(SongBox* song);

    virtual void handle_input(double current_ms);

    virtual void handle_input_browsing(double current_ms);
    virtual void handle_input_selecting();
    virtual void handle_input_diff_sorting();
    virtual void handle_input_search();

    virtual void draw_overlays();

    virtual Screens get_game_screen_target() { return Screens::GAME; }

public:
    SongSelectScreen() : Screen("song_select") {
    }

    explicit SongSelectScreen(const std::string& name) : Screen(name) {}

    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
