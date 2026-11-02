#include "tutorial.h"

#include "../libs/input/input.h"

static const fs::path DEFAULT_TUTORIAL_PATH = fs::path("Songs/00 Tutorial") / TUTORIAL_CHART;

namespace {
    class TutorialPlayer : public Player {
        std::vector<TutorialScreen::Hit>& hits;

    public:
        TutorialPlayer(std::optional<SongParser>& parser, PlayerNum player_num, const Modifiers& modifiers,
                       std::vector<TutorialScreen::Hit>& hits)
            : Player(parser, player_num, static_cast<int>(Difficulty::EASY), false, modifiers), hits(hits) {
            if (tex.has_texture("tutorial/wakaba")) {
                lane_mark = tex.get_texture("tutorial/wakaba");
            }
            show_lane_name = false;
        }

        void spawn_hit_effects(DrumType drum_type, Side side) override {
            Player::spawn_hit_effects(drum_type, side);
            hits.push_back({drum_type == DrumType::KAT, side == Side::RIGHT});
        }
    };
} // namespace

Modifiers TutorialScreen::get_player_modifiers(PlayerNum pn) {
    Modifiers mods;
    mods.auto_play = true;
    return mods;
}

void TutorialScreen::create_players() {
    players.push_back(std::make_unique<TutorialPlayer>(parser, global_data.player_num,
                                                       get_player_modifiers(global_data.player_num), hits));
}

void TutorialScreen::on_screen_start() {
    SessionData& session_data = global_data.session_data[static_cast<int>(global_data.player_num)];
    prev_song = session_data.selected_song;
    prev_difficulty = session_data.selected_difficulty;
    session_data.selected_song =
        exists(session_data.selected_tutorial) ? session_data.selected_tutorial : DEFAULT_TUTORIAL_PATH;
    session_data.selected_difficulty = static_cast<int>(Difficulty::EASY);
    hits.clear();
    rim_count = 0;
    GameScreen::on_screen_start();
    transition->set_rainbow_only();
    overlay.emplace();
}

Screens TutorialScreen::on_screen_end(Screens next_screen) {
    overlay.reset();
    SessionData& session_data = global_data.session_data[static_cast<int>(global_data.player_num)];
    session_data.selected_song = prev_song;
    session_data.selected_difficulty = prev_difficulty;
    return GameScreen::on_screen_end(next_screen);
}

std::optional<Screens> TutorialScreen::update() {
    if (auto init = Screen::update()) {
        return init;
    }

    double current_ms = get_frame_ms();
    allnet_indicator.update(current_ms);
    clock.update(current_ms);
    transition->update(current_ms);
    update_gameplay(current_ms);

    const double ms = chart_ms();
    overlay->update(ms);
    for (const Hit& hit : hits) {
        overlay->hit(hit.kat, hit.right, ms);
    }
    hits.clear();

    while (is_l_kat_pressed(global_data.player_num) || is_r_kat_pressed(global_data.player_num)) {
        rim_count++;
    }
    bool finished = overlay->loaded() ? overlay->is_finished(ms) : clock.position() >= players[0]->end_time;
    if (rim_count >= SKIP_HITS || finished) {
        return on_screen_end(Screens::SONG_SELECT);
    }

    if (auto fill = overlay->gauge(ms); fill && players[0]->gauge) {
        players[0]->gauge->set_fraction(*fill);
    }

    return global_keys();
}

void TutorialScreen::draw() {
    clear_background(Color::BLACK);
    if (background.has_value()) {
        background->draw_back();
    }
    const double ms = chart_ms();
    overlay->draw_back(ms);
    players[0]->draw(clock.position(), 0, LANE_Y * tex.screen_scale);
    if (background.has_value()) {
        background->draw_fore();
    }
    overlay->draw(ms, rim_count);
    draw_overlay(false);
}
