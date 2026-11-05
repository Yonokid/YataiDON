#include "ai_battle.h"

#include <algorithm>
#include <array>
#include <memory>

#include "../libs/input/input.h"

static constexpr int LEVEL_SELECT_SECONDS = 10;

static constexpr std::array<int, 4> FIRST_LEVEL = {1, 2, 6, 10};
static constexpr std::array<int, 4> LEVEL_COUNT = {1, 4, 4, 4};

int AISongSelectScreen::hovered_course() const {
    if (!menu || !menu->loaded() || state != SongSelectState::SONG_SELECTED) {
        return -1;
    }
    const auto course = static_cast<int>(player->selected_difficulty);
    return course >= 0 && course < static_cast<int>(LEVEL_COUNT.size()) ? course : -1;
}

void AISongSelectScreen::on_screen_start() {
    SongSelectScreen::on_screen_start();
    global_data.ai_condition = ai_roll_condition();
    menu.emplace();
    choosing_level = false;
    level_decided = false;
    level_cursor = 0;
}

Screens AISongSelectScreen::on_screen_end(Screens next_screen) {
    menu.reset();
    return SongSelectScreen::on_screen_end(next_screen);
}

void AISongSelectScreen::handle_input_selecting() {
    if (choosing_level) {
        if (level_decided) {
            clear_input_buffers();
            return;
        }
        const int last = LEVEL_COUNT[chosen_course] - 1;
        if (is_l_kat_pressed(global_data.player_num)) {
            audio.play_player_sound("kat", global_data.player_num, VolumePreset::SOUND);
            level_cursor = std::max(level_cursor - 1, 0);
        } else if (is_r_kat_pressed(global_data.player_num)) {
            audio.play_player_sound("kat", global_data.player_num, VolumePreset::SOUND);
            level_cursor = std::min(level_cursor + 1, last);
        } else if (is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num)) {
            audio.play_player_sound("don", global_data.player_num, VolumePreset::SOUND);
            level_decided = true;
        }
        return;
    }

    SongSelectScreen::handle_input_selecting();

    const int course = hovered_course();
    if (course >= 0 && player->difficulty_decided() && LEVEL_COUNT[course] > 1) {
        chosen_course = course;
        choosing_level = true;
        level_cursor = LEVEL_COUNT[course] - 1;
        diff_select_timer =
            std::make_unique<Timer>(LEVEL_SELECT_SECONDS, get_current_ms(), [this] { level_decided = true; });
    }
}

void AISongSelectScreen::select_song(SongBox* song) {
    if (choosing_level && !level_decided) {
        return;
    }
    const bool simplified = choosing_level && level_cursor < LEVEL_COUNT[chosen_course] - 1;
    const auto human = static_cast<int>(global_data.player_num);
    global_data.session_data[human].subdiff = simplified ? FIRST_LEVEL[chosen_course] + level_cursor : 0;

    SongSelectScreen::select_song(song);
}

void AISongSelectScreen::draw_overlays() {
    if (menu) {
        const int course = hovered_course();
        const auto* song = dynamic_cast<SongBox*>(navigator.get_current_item());
        const bool branching = course >= 0 && song != nullptr && song->course_info(course).is_branching;
        menu->draw(get_current_ms(), course, level_cursor, choosing_level, branching,
                   static_cast<int>(global_data.player_num));
    }
    SongSelectScreen::draw_overlays();
}

static constexpr int HUMAN = 0;
static constexpr int AI = 1;

static PlayerNum other_seat(PlayerNum seat) {
    return seat == PlayerNum::P1 ? PlayerNum::P2 : PlayerNum::P1;
}

Player* AIGameScreen::human() const {
    return players[global_data.player_num == PlayerNum::P1 ? 0 : 1].get();
}

Player* AIGameScreen::ai() const {
    return players[global_data.player_num == PlayerNum::P1 ? 1 : 0].get();
}

Modifiers AIGameScreen::get_player_modifiers(PlayerNum pn) {
    Modifiers modifiers = Game2PScreen::get_player_modifiers(global_data.player_num);
    if (pn != global_data.player_num) {
        modifiers.auto_play = true;
    }
    modifiers.subdiff = global_data.session_data[static_cast<int>(global_data.player_num)].subdiff;
    modifiers.no_kusudama = true;
    return modifiers;
}

void AIGameScreen::initialize_chart(const fs::path& song) {
    const auto seat = static_cast<int>(global_data.player_num);
    global_data.session_data[static_cast<int>(other_seat(global_data.player_num))] = global_data.session_data[seat];
    Game2PScreen::initialize_chart(song);
    start_match();
    apply_overlay_layout();
}

void AIGameScreen::start_match() {
    const SessionData& session = global_data.session_data[static_cast<int>(global_data.player_num)];
    auto [notes, branch_m, branch_e, branch_n] = parser->notes_to_position(session.selected_difficulty);

    std::vector<double> note_ms;
    std::vector<double> barline_ms;
    auto collect = [&](const NoteList& list) {
        for (const Note& note : list.notes) {
            if (note.type == NoteType::BARLINE) {
                barline_ms.push_back(note.hit_ms);
            } else if (note.type >= NoteType::DON && note.type <= NoteType::KAT_L) {
                note_ms.push_back(note.hit_ms);
            }
        }
    };
    collect(notes);
    for (const NoteList& section : branch_m) {
        collect(section);
    }
    std::ranges::sort(note_ms);
    std::ranges::sort(barline_ms);

    section_count = ai_section_count(human()->end_time - start_delay);
    section_starts = ai_section_starts(note_ms, barline_ms, section_count);

    note_section.clear();
    auto assign = [&](const NoteList& list) {
        for (const Note& note : list.notes) {
            note_section[note.index] = ai_section_of(section_starts, note.hit_ms);
        }
    };
    assign(notes);
    for (const auto* branch : {&branch_m, &branch_e, &branch_n}) {
        for (const NoteList& section : *branch) {
            assign(section);
        }
    }

    for (auto& tally : tallies) {
        tally.assign(section_count, AITally{});
    }
    player_total = {};
    current_section = 0;
    sections_won = 0;
    match_recorded = false;

    brain = AIBrain{.condition = global_data.ai_condition};
    ai()->autoplay_judge = [this](const Note&) {
        return ai_pick_judgment(brain.rates(player_total), std::uniform_real_distribution<double>(0.0, 1.0)(rng));
    };
    for (auto& player : players) {
        player->hide_gogo_flame = true;
        player->hide_puchi();
    }
    if (overlay) {
        overlay->reset();
    }
}

void AIGameScreen::apply_overlay_layout() {
    if (!overlay_active()) {
        return;
    }
    background.reset();
    movie.reset();
    for (auto& player : players) {
        player->place_chara_on_stage(0.0f, 0.0f);
        player->rainbow_arc = true;
    }
    human()->gauge->use_script("AIGauge", "ai_battle");
    ai()->hide_gauge = true;
    ai()->hide_score_counter = true;
    ai()->hide_lane_badges();
}

void AIGameScreen::on_screen_start() {
    Game2PScreen::on_screen_start();
    overlay.emplace();
    if (overlay->loaded()) {
        overlay->reset();
    }
    apply_overlay_layout();
}

Screens AIGameScreen::on_screen_end(Screens next_screen) {
    overlay.reset();
    return Game2PScreen::on_screen_end(next_screen);
}

void AIGameScreen::update_tallies() {
    auto recount = [this](const Player& player, std::vector<AITally>& tally) {
        tally.assign(section_count, AITally{});
        auto section_of = [this](int index) {
            const auto it = note_section.find(index);
            return it == note_section.end() ? -1 : it->second;
        };
        for (const auto& [index, judgment] : player.get_note_judgments()) {
            const int section = section_of(index);
            if (section < 0) {
                continue;
            }
            int& count = judgment == Judgments::GOOD ? tally[section].good
                         : judgment == Judgments::OK ? tally[section].ok
                                                     : tally[section].bad;
            ++count;
        }
        for (const auto& [index, hits] : player.get_roll_hits()) {
            const int section = section_of(index);
            if (section >= 0) {
                tally[section].drumroll += hits;
            }
        }
    };
    recount(*human(), tallies[HUMAN]);
    recount(*ai(), tallies[AI]);
    player_total = {};
    for (const AITally& tally : tallies[HUMAN]) {
        player_total.good += tally.good;
        player_total.ok += tally.ok;
        player_total.bad += tally.bad;
    }
}

double AIGameScreen::section_end_ms(int section) const {
    return section < static_cast<int>(section_starts.size()) ? section_starts[section] : human()->end_time;
}

int AIGameScreen::current_territory() const {
    const int section = std::min(current_section, section_count - 1);
    const int course = global_data.session_data[static_cast<int>(global_data.player_num)].selected_difficulty;
    return ai_territory_points(course, tallies[HUMAN][section]) - ai_territory_points(course, tallies[AI][section]);
}

void AIGameScreen::close_finished_sections() {
    while (current_section < section_count && clock.position() >= section_end_ms(current_section) + Timing::BAD_EASY) {
        const int margin = current_territory();
        const bool won = margin >= 0; // a tie goes to the player
        sections_won += won;
        brain.on_section_end(margin);
        if (overlay) {
            overlay->set_section_result(current_section, won);
        }
        ++current_section;
    }
    if (current_section == section_count && !match_recorded) {
        match_recorded = true;
        SessionData& session = global_data.session_data[static_cast<int>(global_data.player_num)];
        session.ai_sections_won = sections_won;
        session.ai_sections_total = section_count;
        spdlog::info("AI battle over: player won {} of {} sections", sections_won, section_count);
    }
}

std::optional<Screens> AIGameScreen::update() {
    auto next = Game2PScreen::update();
    if (next || players.size() != 2) {
        return next;
    }
    // recounts every note each frame;
    update_tallies();
    close_finished_sections();
    return next;
}

void AIGameScreen::draw() {
    if (!overlay_active()) {
        Game2PScreen::draw();
        return;
    }
    clear_background(Color::BLACK);
    const int territory = std::clamp(current_territory(), -AI_TERRITORY_MAX, AI_TERRITORY_MAX);
    const double ms = get_current_ms();
    overlay->draw_back(ms, territory);

    static constexpr float TILE_WIDTH = 72.0f;
    static constexpr float CHARA_Y = 180.0f;
    static constexpr float CHARA_GAP = 80.0f;
    static constexpr float CHARA_SCALE = 0.35f;
    const float front = (territory + AI_TERRITORY_MAX + 1) * TILE_WIDTH;
    human()->move_stage_chara((front - CHARA_GAP) * tex.screen_scale, CHARA_Y * tex.screen_scale);
    ai()->move_stage_chara((front + CHARA_GAP) * tex.screen_scale, CHARA_Y * tex.screen_scale);
    human()->draw_stage_chara(CHARA_SCALE);
    ai()->draw_stage_chara(CHARA_SCALE);

    const int shown_section = std::min(current_section, section_count - 1);
    const double section_start = shown_section == 0 ? start_delay : section_starts[shown_section - 1];
    const double section_length = std::max(1.0, section_end_ms(shown_section) - section_start);
    const auto progress = static_cast<float>(std::clamp((clock.position() - section_start) / section_length, 0.0, 1.0));
    overlay->draw_front(ms, current_section, section_count, progress);

    draw_players();

    for (const bool is_ai : {false, true}) {
        const AITally& tally = tallies[is_ai ? AI : HUMAN][shown_section];
        overlay->draw_scoreboard(is_ai, tally.good, tally.ok, tally.bad, tally.drumroll);
    }
    ai()->draw_arcs();
    draw_overlay();
}
