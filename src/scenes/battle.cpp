#include "battle.h"

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
    global_data.battle_stage = status.answered ? std::min(status.stage, BATTLE_STAGES_PORTED) : 1;
    const auto boss = status.bosses.find(global_data.battle_stage);
    global_data.battle_boss_hp = boss == status.bosses.end() ? 0 : boss->second.hp;
    global_data.battle_boss_hp_max = boss == status.bosses.end() ? 0 : boss->second.max;
    SongSelectScreen::on_screen_start();
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
    skill_lands_ms.reset();
    ended = false;
    boss_hp = global_data.battle_boss_hp;
    field.clear();
    while (!boss && std::ssize(field) < BATTLE_FIELD_SIZE) {
        field.push_back(battle_next_zako(rng));
    }
    enemy_hp = boss ? 0 : BATTLE_ZAKO_HP[field.front()];
    players[0]->on_note_judged = [this](const Note& note, Judgments judgment) { on_note(note, judgment); };
    announce_battle();
}

void BattleGameScreen::announce_battle() {
    if (!overlay_active()) {
        return;
    }
    overlay->reset(JudgePos::X, JudgePos::Y);
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
    player.hide_score_counter = true;
    player.hide_judgment_text = true;
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
    field.push_back(battle_next_zako(rng));
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
        field.pop_front();
        if (overlay_active()) {
            overlay->kill(get_current_ms(), skill);
        }
        pop_next();
        enemy_hp = BATTLE_ZAKO_HP[field.front()];
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
    if (!skill_lands_ms && skill_notes >= battle_skill_threshold(difficulty, skills_fired)) {
        ++skills_fired;
        skill_lands_ms = now + SKILL_HIT_MS;
        if (overlay_active()) {
            overlay->start_skill(now, 1);
        }
        if (std::uniform_real_distribution<double>(0.0, 1.0)(rng) < BATTLE_CHEST_CHANCE) {
            ++chests;
            if (overlay_active()) {
                overlay->drop_chest(now);
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
        submit_song();
        if (overlay_active()) {
            overlay->song_end(now);
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
    network.submit_battle(code, session.song_hash, difficulty, stage, skill_notes, total_damage,
                          total_damage / BATTLE_POWER_DAMAGE, chests);
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
    static constexpr float CHARA_SCALE = 0.6f;
    players[0]->move_stage_chara(CHARA_X * tex.screen_scale, CHARA_Y * tex.screen_scale);
    players[0]->draw_stage_chara(CHARA_SCALE);
    overlay->draw_cutin(ms);

    draw_players();
    overlay->draw_front(ms);
    draw_overlay();
}
