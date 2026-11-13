#include "enso_battle.h"

#include "../objects/game/enso_match.h"

// the special move's slash lands this long after the cut-in starts
static constexpr double SKILL_HIT_MS = 1300.0;

void EnsoGameScreen::initialize_chart(const fs::path& song) {
    GameScreen::initialize_chart(song);
    start_battle();
    apply_overlay_layout();
}

void EnsoGameScreen::start_battle() {
    attack = enso_base_attack(ENSO_DEFAULT_BOND);
    difficulty = global_data.session_data[static_cast<int>(global_data.player_num)].selected_difficulty;
    total_damage = 0;
    skill_notes = 0;
    skills_fired = 0;
    rolls_seen = 0;
    skill_lands_ms.reset();
    ended = false;
    if (overlay_active()) {
        overlay->reset(JudgePos::X, JudgePos::Y);
    }
    field.clear();
    while (std::ssize(field) < ENSO_FIELD_SIZE) {
        pop_next();
    }
    enemy_hp = ENSO_ZAKO_HP[field.front()];
    players[0]->on_note_judged = [this](const Note& note, Judgments judgment) { on_note(note, judgment); };
}

void EnsoGameScreen::apply_overlay_layout() {
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
    player.place_chara_on_stage(0.0f, 0.0f);
}

void EnsoGameScreen::on_screen_start() {
    GameScreen::on_screen_start();
    overlay.emplace();
    if (overlay->loaded()) {
        overlay->reset(JudgePos::X, JudgePos::Y);
        for (const int kind : field) {
            overlay->pop_enemy(get_current_ms(), kind);
        }
    }
    apply_overlay_layout();
}

Screens EnsoGameScreen::on_screen_end(Screens next_screen) {
    overlay.reset();
    return GameScreen::on_screen_end(next_screen);
}

void EnsoGameScreen::pop_next() {
    field.push_back(enso_next_zako(rng));
    if (overlay_active()) {
        overlay->pop_enemy(get_current_ms(), field.back());
    }
}

void EnsoGameScreen::deal(int damage, bool skill) {
    total_damage += damage;
    while (damage >= enemy_hp) {
        damage -= enemy_hp;
        field.pop_front();
        if (overlay_active()) {
            overlay->kill(get_current_ms(), skill);
        }
        pop_next();
        enemy_hp = ENSO_ZAKO_HP[field.front()];
    }
    enemy_hp -= damage;
}

void EnsoGameScreen::on_note(const Note& note, Judgments judgment) {
    if (note.type < NoteType::DON || note.type > NoteType::KAT_L || ended) {
        return;
    }
    const EnsoHit hit = enso_note_damage(attack, judgment, rng);
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
    if (!skill_lands_ms && skill_notes >= enso_skill_threshold(difficulty, skills_fired)) {
        ++skills_fired;
        skill_lands_ms = now + SKILL_HIT_MS;
        if (overlay_active()) {
            overlay->start_skill(now, 1);
        }
        if (std::uniform_real_distribution<double>(0.0, 1.0)(rng) < ENSO_CHEST_CHANCE) {
            if (overlay_active()) {
                overlay->drop_chest(now);
            }
        }
    }
}

void EnsoGameScreen::update_battle() {
    const double now = get_current_ms();
    const int rolls = players[0]->result().drumroll;
    for (; rolls_seen < rolls; ++rolls_seen) {
        const int damage = enso_roll_damage(attack, rng);
        deal(damage);
        if (overlay_active()) {
            overlay->roll_hit(now, damage);
        }
    }
    if (skill_lands_ms && now >= *skill_lands_ms) {
        skill_lands_ms.reset();
        deal(attack * ENSO_SKILL_ATTACKS, true);
    }
    if (!ended && clock.position() >= players[0]->end_time) {
        ended = true;
        if (overlay_active()) {
            overlay->song_end(now);
        }
    }
    if (overlay_active()) {
        const int fired_before = skill_lands_ms ? skills_fired - 1 : skills_fired;
        const float fill = skill_lands_ms ? 1.0f : enso_skill_fill(difficulty, fired_before, skill_notes);
        overlay->set_status(total_damage / ENSO_POWER_DAMAGE, fill);
    }
}

std::optional<Screens> EnsoGameScreen::update() {
    auto next = GameScreen::update();
    if (next || players.empty()) {
        return next;
    }
    update_battle();
    return next;
}

void EnsoGameScreen::draw() {
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
