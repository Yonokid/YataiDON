#include "ai_battle.h"

#include <algorithm>
#include <array>
#include <memory>

#include "../libs/file/scores.h"
#include "../libs/input/input.h"

static constexpr int LEVEL_SELECT_SECONDS = 10;

static constexpr std::array<int, 4> FIRST_LEVEL = {1, 2, 6, 10};
static constexpr std::array<int, 4> LEVEL_COUNT = {1, 4, 4, 4};

int AISongSelectScreen::hovered_course() const {
    if (!menu || !menu->loaded() || state != SongSelectState::SONG_SELECTED || tex.options[SCO::AI_NO_SUPPORT_LEVELS]) {
        return -1;
    }
    const auto course = static_cast<int>(player->selected_difficulty);
    return course >= 0 && course < static_cast<int>(LEVEL_COUNT.size()) ? course : -1;
}

void AISongSelectScreen::play_call_voice() {
    audio.stop_sound("voice_enter");
    const bool kadai = status.answered && status.rank == AI_ASSIGNED_RANK;
    audio.play_sound(kadai ? "ai_select_vo_kadai" : "ai_select_vo_call", VolumePreset::VOICE);
}

void AISongSelectScreen::on_screen_start() {
    const std::string& code = global_data.player_num == PlayerNum::P1 ? global_data.config->network.access_code_1
                                                                      : global_data.config->network.access_code_2;
    status = code.empty() ? AIBattleStatus{} : network.fetch_ai_battle_status(code);
    SongSelectScreen::on_screen_start();
    PlayerData ai_data;
    ai_data.chara_cos_index = AI_COSTUME;
    ai_chara = make_chara_from_player_data(&ai_data, global_data.player_num == PlayerNum::P1);
    ai_chara->set_anim(AnimIndex::DON_SELECT_LOOP);
    global_data.ai_condition = ai_roll_condition();
    menu.emplace();
    menu->set_status(status);
    choosing_level = false;
    level_decided = false;
    level_cursor = 0;
    if (script_manager.has_lua_script("ai_intro") &&
        scores_manager.first_visit(get_player_id(global_data.player_num), "ai_intro_seen")) {
        intro = std::make_unique<AIIntro>();
        if (intro->unavailable()) {
            intro.reset();
        } else {
            audio.stop_sound("bgm");
            audio.stop_sound("voice_enter");
        }
    }
    if (!intro) {
        play_call_voice();
    }
}

Screens AISongSelectScreen::on_screen_end(Screens next_screen) {
    menu.reset();
    intro.reset();
    ai_chara.reset();
    return SongSelectScreen::on_screen_end(next_screen);
}

std::optional<Screens> AISongSelectScreen::update() {
    if (!intro) {
        auto next = SongSelectScreen::update();
        if (ai_chara) {
            ai_chara->update(get_current_ms());
        }
        return next;
    }
    if (auto next = Screen::update()) {
        return next;
    }
    const double current_ms = get_current_ms();
    if (menu_left_pressed()) {
        intro->input(AIIntro::LEFT);
    } else if (menu_right_pressed()) {
        intro->input(AIIntro::RIGHT);
    } else if (menu_confirm_pressed()) {
        intro->input(AIIntro::DECIDE);
    }
    intro->update(current_ms);
    if (intro->is_done()) {
        intro.reset();
        audio.play_sound("bgm", VolumePreset::MUSIC);
        play_call_voice();
        select_timer = std::make_unique<Timer>(100, current_ms, [this] { on_select_timeout(); });
    }
    return std::nullopt;
}

void AISongSelectScreen::draw() {
    if (intro) {
        intro->draw();
        return;
    }
    SongSelectScreen::draw();
}

void AISongSelectScreen::handle_input_selecting() {
    if (choosing_level) {
        if (level_decided) {
            clear_input_buffers();
            return;
        }
        const int last = LEVEL_COUNT[chosen_course] - 1;
        if (is_l_kat_pressed(global_data.player_num)) {
            audio.play_player_sound("kat", global_data.player_num, VolumePreset::HITSOUND);
            level_cursor = std::max(level_cursor - 1, 0);
        } else if (is_r_kat_pressed(global_data.player_num)) {
            audio.play_player_sound("kat", global_data.player_num, VolumePreset::HITSOUND);
            level_cursor = std::min(level_cursor + 1, last);
        } else if (is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num)) {
            audio.play_player_sound("don", global_data.player_num, VolumePreset::HITSOUND);
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
    global_data.session_data[human].ai_assigned = song->collection == AI_ASSIGNED_COLLECTION;

    SongSelectScreen::select_song(song);
    audio.play_sound("ai_select_vo_begin", VolumePreset::VOICE);
}

void AISongSelectScreen::draw_overlays() {
    if (ai_chara) {
        auto [chara_x, chara_y] =
            tex.chara_xy(global_data.player_num == PlayerNum::P1 ? "song_select_2p" : "song_select_1p");
        ai_chara->draw(chara_x, chara_y);
    }
    if (menu) {
        const int course = hovered_course();
        const auto* song = dynamic_cast<SongBox*>(navigator.get_current_item());
        const bool branching = course >= 0 && song != nullptr && song->course_info(course).is_branching;
        menu->draw(get_current_ms(), course, level_cursor, choosing_level, branching,
                   static_cast<int>(global_data.player_num), ai_condition_tier(global_data.ai_condition));
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
    ai_hits_seen = 0;
    section_won.clear();
    global_data.ai_match = {};

    brain = AIBrain{.condition = global_data.ai_condition};
    ai()->use_costume(AI_COSTUME);
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
        player->hide_ending_anim = true;
    }
    human()->gauge->use_script("AIGauge", "ai_battle");
    ai()->hide_gauge = true;
    ai()->hide_score_counter = true;
    ai()->hide_lane_badges();
    ai()->use_ai_lane();
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
            ai_count_judgment(tally[section], judgment);
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
    auto judged_in_section = [&](const Player& player) {
        std::vector<std::pair<int, Judgments>> notes;
        for (const auto& [index, judgment] : player.get_note_judgments()) {
            const auto it = note_section.find(index);
            if (it != note_section.end() && it->second == section) {
                notes.emplace_back(index, judgment);
            }
        }
        std::ranges::sort(notes);
        std::vector<Judgments> judgments;
        judgments.reserve(notes.size());
        for (const auto& [index, judgment] : notes) {
            judgments.push_back(judgment);
        }
        return judgments;
    };
    return ai_territory_margin(course, judged_in_section(*human()), judged_in_section(*ai()));
}

void AIGameScreen::close_finished_sections() {
    while (current_section < section_count &&
           (skipped || clock.position() >= section_end_ms(current_section) + Timing::BAD_EASY)) {
        const int margin = current_territory();
        const bool won = margin >= 0 && !skipped; // a tie goes to the player; a skip leaves the rest to the AI
        sections_won += won;
        section_won.push_back(won);
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
        record_match(session);
        if (overlay) {
            overlay->set_match_result(sections_won * 2 > section_count, human()->get_bad() == 0);
        }
    }
}

void AIGameScreen::record_match(const SessionData& session) {
    AIMatchResult& match = global_data.ai_match;
    match.sections.clear();
    for (int section = 0; section < section_count; ++section) {
        const AITally& tally = tallies[HUMAN][section];
        const bool won = section_won[section];
        match.sections.push_back({tally.good, tally.ok, tally.bad, tally.drumroll, won});
    }
    match.won = sections_won * 2 > section_count;
    match.saves = !human()->is_auto_play() && !human()->is_replay() && !skipped && session.subdiff == 0;
    const auto& courses = parser->metadata.course_data;
    const auto course = courses.find(session.selected_difficulty);
    match.stars = std::clamp(course == courses.end() ? 1 : static_cast<int>(course->second.level), 1, 10);
    network.reset_ai_battle();
    if (!match.saves) {
        return;
    }
    scores_manager.save_ai_section_wins(session.song_hash, session.selected_difficulty,
                                        get_player_id(global_data.player_num), sections_won, section_count);
    const std::string& code = global_data.player_num == PlayerNum::P1 ? global_data.config->network.access_code_1
                                                                      : global_data.config->network.access_code_2;
    if (code.empty()) {
        return;
    }
    std::string sections;
    for (const AISectionTally& s : match.sections) {
        sections += fmt::format("{}{}:{}:{}:{}:{}", sections.empty() ? "" : ",", s.good, s.ok, s.bad, s.drumroll,
                                s.won ? 1 : 0);
    }
    network.submit_ai_battle(code, session.song_hash, session.selected_difficulty, match.stars, match.won,
                             session.ai_assigned, sections);
}

// screen option ai_wait_loading_card: the chart waits behind the loading card, and nothing typed meanwhile reaches the
// drums
void AIGameScreen::update_loading_hold() {
    const bool card_up = tex.options[SCO::AI_WAIT_LOADING_CARD] && transition && !transition->is_finished();
    if (card_up != card_hold) {
        card_hold = card_up;
        card_up ? clock.pause(get_current_ms()) : clock.resume(get_current_ms());
        clear_input_buffers();
    } else if (card_up) {
        clear_input_buffers();
    }
}

// a skin whose overlay has handle_skip offers the skip: read from the AI's unplayed lane, as in a solo play
void AIGameScreen::update_ai_skip() {
    if (!overlay_active() || !overlay->wants_skip()) {
        return;
    }
    if (skip_lane == PlayerNum::ALL && !skipped && human()->is_skip_enabled()) {
        skip_lane = other_seat(global_data.player_num);
    }
    const size_t ai_hits = ai()->input_log.size();
    skip_play_hits += ai_hits - ai_hits_seen; // only the player's own hits cancel a skip
    ai_hits_seen = ai_hits;
    update_skip();
    skip_text.reset();
    if (skip_lane != PlayerNum::ALL) {
        overlay->handle_skip(skip_count, SKIP_HITS, skipped);
    }
}

std::optional<Screens> AIGameScreen::update() {
    update_loading_hold();
    auto next = Game2PScreen::update();
    if (next || players.size() != 2) {
        return next;
    }
    update_ai_skip();
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

    static constexpr float CHARA_Y = 147.0f;
    static constexpr float HUMAN_CHARA_GAP = 101.0f;
    static constexpr float AI_CHARA_GAP = 50.0f;
    static constexpr float CHARA_SCALE = 0.5f;
    const float front = overlay->border_x(territory);
    human()->move_stage_chara((front - HUMAN_CHARA_GAP) * tex.screen_scale, CHARA_Y * tex.screen_scale);
    ai()->move_stage_chara((front + AI_CHARA_GAP) * tex.screen_scale, CHARA_Y * tex.screen_scale);
    human()->draw_stage_chara(CHARA_SCALE);
    ai()->draw_stage_chara(CHARA_SCALE);
    overlay->draw_clash(ms, territory, brain.anger > 0);

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
    overlay->draw_finale(ms);
    if (const SkinInfo* pos = tex.get_skin_config(SC::AI_GAME_SONG_INFO)) {
        song_info.draw(pos->x, pos->y);
        draw_overlay(false);
    } else {
        draw_overlay();
    }
}

void AIResultScreen::on_screen_start() {
    ResultScreen::on_screen_start();
    allow_retry = false;
    board.reset();
    flow.reset();
    board_started = false;
    if (global_data.ai_match.sections.empty() || !script_manager.has_lua_script("ai_section_result")) {
        return;
    }
    audio.stop_sound("bgm");
    audio.load_screen_sounds("ai_section_result");
    board.emplace(global_data.ai_match);
    if (board->unavailable()) {
        board.reset();
        audio.play_sound("bgm", VolumePreset::MUSIC);
        return;
    }
    background.reset();
    if (player_1) {
        player_1->hide_stage();
    }
}

Screens AIResultScreen::on_screen_end(Screens next_screen) {
    flow.reset();
    board.reset();
    global_data.ai_match = {};
    return ResultScreen::on_screen_end(next_screen);
}

double AIResultScreen::reveal_end_ms() {
    return board ? 0.0 : ResultScreen::reveal_end_ms();
}

static constexpr double AI_ANSWER_TIMEOUT_MS = 5000.0;

std::optional<Screens> AIResultScreen::update() {
    auto next = ResultScreen::update();
    if (next || !board || exiting()) {
        return next;
    }
    const double now = get_current_ms();
    board->update(now);
    if (!board_started) {
        const double revealed = ResultScreen::reveal_end_ms();
        if (revealed <= 0 || (network.ai_battle_pending() && now < revealed + AI_ANSWER_TIMEOUT_MS)) {
            return next;
        }
        board_started = true;
        // the board takes the drum from here; the first hit must not also count as a reveal skip
        skipped_time = skipped_time > 0 ? skipped_time : now;
        board->begin(network.ai_battle_progress());
        clear_input_buffers();
        return next;
    }
    // the base back-key check is gated on reveal_end_ms(), which is 0 while the board is up
    if (fade_out && check_key_pressed(global_data.config->keys.back_key)) {
        quitting = true;
        start_fade_out();
        audio.play_sound("don", VolumePreset::HITSOUND);
        return next;
    }
    const PlayerNum seat = global_data.player_num;
    const bool don = menu_confirm_pressed(seat);
    const bool kat = is_l_kat_pressed(seat) || is_r_kat_pressed(seat);
    if (flow) {
        flow->update(now);
        if (don && flow->input(AIRewardFlow::DON)) {
            audio.play_sound("don", VolumePreset::HITSOUND);
        } else if (kat && flow->input(AIRewardFlow::KATSU)) {
            audio.play_sound("kat", VolumePreset::HITSOUND);
        }
        if (flow->is_done()) {
            begin_exit();
        }
        return next;
    }
    if (don && board->decide()) {
        audio.play_sound("don", VolumePreset::HITSOUND);
    } else if (kat && board->toggle()) {
        audio.play_sound("kat", VolumePreset::HITSOUND);
    }
    if (board->is_done()) {
        const AIBattleProgress& progress = network.ai_battle_progress();
        if (progress.answered) {
            flow.emplace(progress);
        }
        if (!flow || flow->unavailable() || flow->is_done()) {
            flow.reset();
            begin_exit();
        }
        clear_input_buffers();
    }
    return next;
}

void AIResultScreen::draw() {
    if (!board) {
        ResultScreen::draw();
        return;
    }
    board->draw_background();
    draw_song_info();
    if (player_1) {
        player_1->draw();
    }
    board->draw();
    if (flow) {
        flow->draw();
    }
    draw_overlay();
}
