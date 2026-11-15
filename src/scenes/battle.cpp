#include "battle.h"

#include "../libs/input/input.h"
#include "../objects/game/battle_match.h"

// the special move's slash lands this long after the cut-in starts
static constexpr double SKILL_HIT_MS = 1300.0;

static const std::string& battle_access_code() {
    return global_data.player_num == PlayerNum::P1 ? global_data.config->network.access_code_1
                                                   : global_data.config->network.access_code_2;
}

void BattleSongSelectScreen::on_screen_start() {
    const std::string& code = battle_access_code();
    const BattleStatus status = code.empty() ? BattleStatus{} : network.fetch_battle_status(code);
    global_data.battle_tracked = status.answered;
    global_data.battle_bond = status.answered ? status.bond : BATTLE_DEFAULT_BOND;
    global_data.battle_best_power = status.best_power;
    global_data.battle_stage = status.answered ? status.stage : 1;
    const auto boss = status.bosses.find(global_data.battle_stage);
    global_data.battle_boss_hp = boss == status.bosses.end() ? 0 : boss->second.hp;
    global_data.battle_boss_hp_max = boss == status.bosses.end() ? 0 : boss->second.max;
    global_data.battle_moves = status.answered ? status.moves : 1;
    global_data.battle_hacchake = status.orb_rewards > 0;
    global_data.battle_orbs = status.orb_cost > 0;
    if (status.answered && global_data.battle_skills.empty()) {
        global_data.battle_skills = status.skills;
    }
    global_data.hold_genre_voice = !status.story.empty() && script_manager.has_lua_script("battle_story");
    SongSelectScreen::on_screen_start();
    if (audio.has_sound("voice_battle_begin")) {
        player->start_voice = "voice_battle_begin";
    }
    const int side = global_data.player_num == PlayerNum::P2 ? 2 : 1;
    info.emplace(status, global_data.battle_stage, side);
    skills.emplace(global_data.battle_moves, global_data.battle_hacchake, side);
    picker.reset();
    if (!status.story.empty() && script_manager.has_lua_script("battle_story")) {
        story = std::make_unique<StoryScript>("BattleStory", "battle_story", sol::as_table(status.story));
        if (story->unavailable()) {
            story.reset();
        } else {
            audio.stop_sound("bgm");
            audio.stop_sound("voice_enter");
        }
    }
    global_data.hold_genre_voice = story != nullptr;
}

Screens BattleSongSelectScreen::on_screen_end(Screens next_screen) {
    info.reset();
    skills.reset();
    picker.clear();
    story.reset();
    global_data.hold_genre_voice = false;
    return SongSelectScreen::on_screen_end(next_screen);
}

std::optional<Screens> BattleSongSelectScreen::update() {
    if (!story) {
        return SongSelectScreen::update();
    }
    if (auto next = Screen::update()) {
        return next;
    }
    const double current_ms = get_current_ms();
    if (menu_left_pressed()) {
        story->input(StoryScript::LEFT);
    } else if (menu_right_pressed()) {
        story->input(StoryScript::RIGHT);
    } else if (menu_confirm_pressed()) {
        story->input(StoryScript::DECIDE);
    }
    story->update(current_ms);
    if (story->is_done()) {
        network.mark_battle_story(battle_access_code(), story->played());
        story.reset();
        global_data.hold_genre_voice = false;
        audio.play_sound("bgm", VolumePreset::MUSIC);
        select_timer = std::make_unique<Timer>(100, current_ms, [this] { on_select_timeout(); });
    }
    return std::nullopt;
}

void BattleSongSelectScreen::draw() {
    if (story) {
        story->draw();
        return;
    }
    SongSelectScreen::draw();
}

void BattleSongSelectScreen::draw_overlays() {
    if (info) {
        info->draw(get_current_ms());
    }
    picker.draw(get_current_ms(), picker.hovered_course(state, *player),
                dynamic_cast<SongBox*>(navigator.get_current_item()), static_cast<int>(global_data.player_num), -1);
    SongSelectScreen::draw_overlays();
    if (skills) {
        skills->draw(get_current_ms());
    }
}

void BattleSongSelectScreen::handle_input_selecting() {
    const double now = get_current_ms();
    if ((skills && skills->handle_input(now)) || picker.handle_input()) {
        return;
    }
    const PlayerNum pn = global_data.player_num;
    if (skills && state == SongSelectState::SONG_SELECTED && player->selected_difficulty == Difficulty::NEIRO &&
        (is_l_don_pressed(pn) || is_r_don_pressed(pn))) {
        audio.play_player_sound("don", pn, VolumePreset::HITSOUND);
        skills->open(now);
        audio.play_sound("voice_battle_partner", VolumePreset::VOICE);
        return;
    }
    SongSelectScreen::handle_input_selecting();
    if (picker.open_if_decided(picker.hovered_course(state, *player), *player, diff_select_timer, get_current_ms())) {
        audio.play_sound("voice_battle_finediff", VolumePreset::VOICE);
    }
}

void BattleSongSelectScreen::select_song(SongBox* song) {
    if (picker.blocks_select()) {
        return;
    }
    global_data.session_data[static_cast<int>(global_data.player_num)].subdiff = picker.subdiff();
    SongSelectScreen::select_song(song);
}

Modifiers BattleGameScreen::get_player_modifiers(PlayerNum pn) {
    Modifiers modifiers = GameScreen::get_player_modifiers(pn);
    modifiers.subdiff = global_data.session_data[static_cast<int>(pn)].subdiff;
    modifiers.auto_play = global_data.force_auto_play;
    return modifiers;
}

void BattleGameScreen::initialize_chart(const fs::path& song) {
    GameScreen::initialize_chart(song);
    start_battle();
    apply_overlay_layout();
}

void BattleGameScreen::start_battle() {
    stage = global_data.battle_stage;
    boss = battle_is_boss_stage(stage) && global_data.battle_boss_hp_max > 0;
    attack = battle_base_attack(global_data.battle_bond);
    difficulty = global_data.session_data[static_cast<int>(global_data.player_num)].selected_difficulty;
    total_damage = 0;
    skill_notes = 0;
    skills_fired = 0;
    rolls_seen = 0;
    chests = 0;
    orb_chests = 0;
    kills = 0;
    global_data.battle_song = {};
    skill_lands_ms.reset();
    gogo = false;
    skill_slots = global_data.battle_skills;
    ended = false;
    boss_hp = global_data.battle_boss_hp;
    field.clear();
    while (!boss && std::ssize(field) < BATTLE_FIELD_SIZE) {
        field.push_back(battle_next_enemy(stage, rng));
    }
    enemy_hp = boss ? 0 : battle_enemy_hp(stage, field.front());
    players[0]->on_note_judged = [this](const Note& note, Judgments judgment) { on_note(note, judgment); };
    announce_battle();
}

void BattleGameScreen::announce_battle() {
    if (!overlay_active()) {
        return;
    }
    overlay->reset(JudgePos::X, JudgePos::Y, stage, global_data.battle_orbs,
                   global_data.battle_tracked ? global_data.battle_best_power : -1);
    const double now = get_current_ms();
    if (boss) {
        overlay->boss(now, boss_hp, global_data.battle_boss_hp_max);
    }
    for (const int kind : field) {
        overlay->pop_enemy(now, kind);
    }
}

void BattleGameScreen::apply_overlay_layout() {
    if (!overlay_active()) {
        return;
    }
    background.reset();
    movie.reset();
    Player& player = *players[0];
    player.hide_gauge = true;
    player.hide_ending_anim = true;
    player.skip_ending_anim = true;
    player.hide_score_counter = true;
    player.hide_judgment = true;
    player.use_lane_cover(global_data.player_num == PlayerNum::P2 ? "battle_2p_lane_cover" : "battle_1p_lane_cover");
    player.rainbow_arc = true;
    player.fade_arc = true;
    player.place_chara_on_stage(0.0f, 0.0f);
}

void BattleGameScreen::on_screen_start() {
    GameScreen::on_screen_start();
    overlay.emplace();
    announce_battle();
    apply_overlay_layout();
}

Screens BattleGameScreen::on_screen_end(Screens next_screen) {
    overlay.reset();
    return GameScreen::on_screen_end(next_screen);
}

void BattleGameScreen::pop_next() {
    field.push_back(battle_next_enemy(stage, rng));
    if (overlay_active()) {
        overlay->pop_enemy(get_current_ms(), field.back());
    }
}

void BattleGameScreen::deal(int damage, bool skill) {
    total_damage += damage;
    if (boss) {
        boss_hp = std::max(0, boss_hp - damage);
        return;
    }
    while (damage >= enemy_hp) {
        damage -= enemy_hp;
        ++kills;
        field.pop_front();
        if (overlay_active()) {
            overlay->kill(get_current_ms(), skill);
        }
        pop_next();
        enemy_hp = battle_enemy_hp(stage, field.front());
        if (!skill) {
            return;
        }
    }
    enemy_hp -= damage;
}

void BattleGameScreen::on_note(const Note& note, Judgments judgment) {
    if (note.type < NoteType::DON || note.type > NoteType::KAT_L || ended) {
        return;
    }
    const BattleHit hit = battle_note_damage(attack, judgment, rng);
    if (hit.damage == 0) {
        return;
    }
    deal(hit.damage);
    const double now = get_current_ms();
    if (overlay_active()) {
        const bool kat = note.type == NoteType::KAT || note.type == NoteType::KAT_L;
        const bool big = note.type == NoteType::DON_L || note.type == NoteType::KAT_L;
        overlay->hit(now, hit.damage, judgment == Judgments::GOOD, hit.critical, kat, big);
    }
    ++skill_notes;
    if (skill_lands_ms || skill_notes < battle_skill_threshold(difficulty, skills_fired)) {
        return;
    }
    const int move =
        battle_slot_move(skill_slots, global_data.battle_moves, global_data.battle_hacchake, skills_fired, rng);
    if (move >= 0) {
        ++skills_fired;
        skill_lands_ms = now + SKILL_HIT_MS;
        if (overlay_active()) {
            overlay->start_skill(now, move + 1);
        }
        std::uniform_real_distribution<double> roll(0.0, 1.0);
        if (global_data.battle_orbs && orb_chests < BATTLE_MAX_CHESTS && roll(rng) < BATTLE_ORB_CHEST_CHANCE) {
            ++orb_chests;
            if (overlay_active()) {
                overlay->drop_chest(now, true);
            }
        } else if (chests < BATTLE_MAX_CHESTS && roll(rng) < BATTLE_CHEST_CHANCE) {
            ++chests;
            if (overlay_active()) {
                overlay->drop_chest(now, false);
            }
        }
    }
}

void BattleGameScreen::update_battle() {
    const double now = get_current_ms();
    const int rolls = players[0]->result().drumroll;
    for (; rolls_seen < rolls; ++rolls_seen) {
        const int damage = battle_roll_damage(attack, rng);
        deal(damage);
        if (overlay_active()) {
            overlay->roll_hit(now, damage);
        }
    }
    if (skill_lands_ms && now >= *skill_lands_ms) {
        skill_lands_ms.reset();
        deal(attack * BATTLE_SKILL_ATTACKS, true);
    }
    if (!ended && clock.position() >= players[0]->end_time) {
        ended = true;
        global_data.battle_song = {true,
                                   stage,
                                   difficulty,
                                   boss,
                                   global_data.battle_boss_hp,
                                   global_data.battle_boss_hp_max,
                                   kills,
                                   total_damage,
                                   total_damage / BATTLE_POWER_DAMAGE,
                                   chests,
                                   orb_chests};
        submit_song();
        if (overlay_active()) {
            overlay->song_end(now, players[0]->get_bad() == 0);
        }
    }
    if (players[0]->in_gogo_time() != gogo) {
        gogo = !gogo;
        if (overlay_active()) {
            overlay->set_gogo(now, gogo);
        }
    }
    if (overlay_active()) {
        const int fired_before = skill_lands_ms ? skills_fired - 1 : skills_fired;
        const float fill = skill_lands_ms ? 1.0f : battle_skill_fill(difficulty, fired_before, skill_notes);
        overlay->set_status(total_damage / BATTLE_POWER_DAMAGE, fill, boss_hp);
    }
}

void BattleGameScreen::submit_song() {
    const std::string& code = battle_access_code();
    const Player& player = *players[0];
    if (!global_data.battle_tracked || code.empty() || player.is_auto_play() || player.is_replay()) {
        return;
    }
    const SessionData& session = global_data.session_data[static_cast<int>(global_data.player_num)];
    network.reset_battle();
    network.submit_battle(code, session.song_hash, difficulty, stage, skill_notes, total_damage,
                          total_damage / BATTLE_POWER_DAMAGE, chests, orb_chests, skill_slots);
}

std::optional<Screens> BattleGameScreen::update() {
    auto next = GameScreen::update();
    if (next || players.empty()) {
        return next;
    }
    update_battle();
    return next;
}

void BattleGameScreen::draw() {
    if (!overlay_active()) {
        GameScreen::draw();
        return;
    }
    clear_background(Color::BLACK);
    const double ms = get_current_ms();
    overlay->draw_back(ms);

    static constexpr float CHARA_X = 330.0f;
    static constexpr float CHARA_Y = 640.0f;
    players[0]->move_stage_chara(CHARA_X * tex.screen_scale, CHARA_Y * tex.screen_scale);
    players[0]->draw_stage_chara();
    overlay->draw_cutin(ms);

    draw_players();
    overlay->draw_front(ms);
    // hide_gauge leaves the hit notes' arcs to the screen
    players[0]->draw_arcs();
    draw_overlay();
}

void BattleResultScreen::on_screen_start() {
    ResultScreen::on_screen_start();
    allow_retry = false;
    board.reset();
    board_started = false;
    const BattleTally& song = global_data.battle_song;
    if (!song.played || !script_manager.has_lua_script("battle_result")) {
        return;
    }
    audio.stop_sound("bgm");
    audio.load_screen_sounds("battle_result");
    board.emplace(song, song_number, global_data.battle_bond);
    if (board->unavailable()) {
        board.reset();
        audio.play_sound("bgm", VolumePreset::MUSIC);
        return;
    }
    background.reset();
    player_1.reset();
    opened_ms = get_current_ms();
}

Screens BattleResultScreen::on_screen_end(Screens next_screen) {
    board.reset();
    global_data.battle_song = {};
    return ResultScreen::on_screen_end(next_screen);
}

static constexpr double BATTLE_ANSWER_TIMEOUT_MS = 5000.0;

std::optional<Screens> BattleResultScreen::update() {
    auto next = ResultScreen::update();
    if (next || !board || exiting()) {
        return next;
    }
    const double now = get_current_ms();
    board->update(now);
    if (!board_started) {
        if ((fade_in && !fade_in->is_finished()) ||
            (network.battle_pending() && now < opened_ms + BATTLE_ANSWER_TIMEOUT_MS)) {
            return next;
        }
        board_started = true;
        board->begin(network.battle_progress());
        clear_input_buffers();
        return next;
    }
    if (fade_out && check_key_pressed(global_data.config->keys.back_key)) {
        quitting = true;
        start_fade_out();
        audio.play_sound("don", VolumePreset::HITSOUND);
        return next;
    }
    if (menu_confirm_pressed(global_data.player_num) && board->decide()) {
        audio.play_sound("don", VolumePreset::HITSOUND);
    }
    if (board->is_done()) {
        begin_exit();
    }
    return next;
}

void BattleResultScreen::draw() {
    if (!board) {
        ResultScreen::draw();
        return;
    }
    board->draw_background();
    board->draw();
    draw_overlay();
}
