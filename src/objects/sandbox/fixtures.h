#include "../../scenes/sandbox.h"
#include "../game/judgment.h"
#include "../game/drum_hit_effect.h"
#include "../game/combo.h"
#include "../game/lane_hit_effect.h"
#include "../game/gauge_hit_effect.h"
#include "../game/gogo_time.h"
#include "../game/score_counter.h"
#include "../game/combo_announce.h"
#include "../game/drumroll_counter.h"
#include "../game/balloon_counter.h"
#include "../game/fireworks.h"
#include "../game/fc_animation.h"
#include "../game/clear_animation.h"
#include "../game/fail_animation.h"

// ─── Existing fixtures ────────────────────────────────────────────────────────

struct JudgmentFixture : public SandboxScreen::Fixture {
    int  type_idx = 0;
    bool big      = false;
    std::optional<Judgment> active;

    JudgmentFixture() { name = "Judgment"; }

    uint32_t anchor_texture_id() override { return HIT_EFFECT::OUTER_GOOD; }

    void reset(double) override { active.reset(); type_idx = 0; big = false; }

    void on_space(double) override {
        static const Judgments types[] = { Judgments::GOOD, Judgments::OK, Judgments::BAD };
        active.emplace(types[type_idx], big);
    }
    void on_tab(double) override { big = !big; }

    void update(double ms) override {
        if (active && active->is_finished()) active.reset();
        if (active) active->update(ms);
    }
    void draw() override { if (active) active->draw(0, 0); }

    std::vector<std::string> type_names() override { return {"GOOD", "OK", "BAD"}; }
    int  get_type()           override { return type_idx; }
    void set_type(int idx, double) override { type_idx = idx; }

    std::vector<std::string> debug_lines() override {
        return {
            std::string("Big    : ") + (big ? "yes" : "no"),
            std::string("Active : ") + (active.has_value() ? "yes" : "no"),
        };
    }
    std::string controls() override {
        return "SPACE/L-CLICK: trigger  R/R-CLICK: reset  VARIANT: toggle big";
    }
};

struct DrumHitFixture : public SandboxScreen::Fixture {
    int variant = 0;
    std::optional<DrumHitEffect> active;

    DrumHitFixture() { name = "DrumHitEffect"; }

    uint32_t anchor_texture_id() override { return LANE::DRUM_DON_L; }

    void reset(double) override { active.reset(); variant = 0; }

    void on_space(double) override {
        static const DrumType dtype[] = { DrumType::DON, DrumType::DON, DrumType::KAT, DrumType::KAT };
        static const Side     dside[] = { Side::LEFT,    Side::RIGHT,   Side::LEFT,    Side::RIGHT   };
        active.emplace(dtype[variant], dside[variant]);
    }

    void update(double ms) override {
        if (active && active->is_finished()) active.reset();
        if (active) active->update(ms);
    }
    void draw() override { if (active) active->draw(0); }

    std::vector<std::string> type_names() override { return {"DON L", "DON R", "KAT L", "KAT R"}; }
    int  get_type()           override { return variant; }
    void set_type(int idx, double) override { variant = idx; }

    std::vector<std::string> debug_lines() override {
        return { std::string("Active : ") + (active.has_value() ? "yes" : "no") };
    }
    std::string controls() override {
        return "SPACE/L-CLICK: trigger  R/R-CLICK: reset";
    }
};

struct ComboFixture : public SandboxScreen::Fixture {
    Combo combo;
    int   count       = 0;
    bool  initialized = false;

    ComboFixture() { name = "Combo"; }

    uint32_t anchor_texture_id() override { return COMBO::COMBO; }

    void reset(double ms) override {
        count = 0;
        combo = Combo(0, ms);
        initialized = true;
    }
    void on_space(double ms) override {
        combo.update(ms, ++count);
        initialized = true;
    }

    void update(double ms) override { if (initialized) combo.update(ms, count); }
    void draw()             override { if (initialized) combo.draw(0); }

    std::vector<std::string> debug_lines() override {
        return { "Combo : " + std::to_string(count) };
    }
    std::string controls() override {
        return "SPACE/L-CLICK: increment combo  R/R-CLICK: reset";
    }
};

struct LaneHitEffectFixture : public SandboxScreen::Fixture {
    int type_idx = 0;
    std::optional<LaneHitEffect> active;

    static constexpr DrumType  dtypes[] = { DrumType::DON, DrumType::DON, DrumType::KAT, DrumType::KAT };
    static constexpr Judgments judgs[]  = { Judgments::GOOD, Judgments::BAD, Judgments::GOOD, Judgments::BAD };

    LaneHitEffectFixture() { name = "LaneHitEffect"; }

    uint32_t anchor_texture_id() override { return LANE::LANE_HIT_EFFECT; }

    void reset(double) override { active.reset(); type_idx = 0; }
    void on_space(double) override { active.emplace(dtypes[type_idx], judgs[type_idx]); }

    void update(double ms) override {
        if (active && active->is_finished()) active.reset();
        if (active) active->update(ms);
    }
    void draw() override { if (active) active->draw(0); }

    std::vector<std::string> type_names() override { return {"DON GOOD", "DON", "KAT GOOD", "KAT"}; }
    int  get_type()           override { return type_idx; }
    void set_type(int idx, double) override { type_idx = idx; }

    std::vector<std::string> debug_lines() override {
        return { std::string("Active : ") + (active.has_value() ? "yes" : "no") };
    }
    std::string controls() override { return "SPACE/L-CLICK: trigger  R/R-CLICK: reset"; }
};

struct GaugeHitEffectFixture : public SandboxScreen::Fixture {
    int  type_idx = 0;
    bool big      = false;
    std::optional<GaugeHitEffect> active;

    GaugeHitEffectFixture() { name = "GaugeHitEffect"; }

    uint32_t anchor_texture_id() override { return GAUGE::HIT_EFFECT; }

    void reset(double) override { active.reset(); type_idx = 0; big = false; }
    void on_space(double) override {
        static const NoteType types[] = { NoteType::DON, NoteType::KAT };
        active.emplace(types[type_idx], big, false);
    }
    void on_tab(double) override { big = !big; }

    void update(double ms) override {
        if (active && active->is_finished()) active.reset();
        if (active) active->update(ms);
    }
    void draw() override { if (active) active->draw(0); }

    std::vector<std::string> type_names() override { return {"DON", "KAT"}; }
    int  get_type()           override { return type_idx; }
    void set_type(int idx, double) override { type_idx = idx; }

    std::vector<std::string> debug_lines() override {
        return {
            std::string("Big    : ") + (big ? "yes" : "no"),
            std::string("Active : ") + (active.has_value() ? "yes" : "no"),
        };
    }
    std::string controls() override { return "SPACE/L-CLICK: trigger  R/R-CLICK: reset  VARIANT: toggle big"; }
};

// ─── Persistent / looping objects ────────────────────────────────────────────

struct GogoTimeFixture : public SandboxScreen::Fixture {
    std::optional<GogoTime> gogo;

    GogoTimeFixture() { name = "GogoTime"; }

    uint32_t anchor_texture_id() override { return GOGO_TIME::FIRE; }

    void reset(double) override { gogo.emplace(); }
    void on_space(double) override { gogo.emplace(); }

    void update(double ms) override { if (gogo) gogo->update(ms); }
    void draw()            override { if (gogo) gogo->draw(0, 0); }

    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: restart"; }
};

struct ScoreCounterFixture : public SandboxScreen::Fixture {
    std::optional<ScoreCounter> counter;
    int score = 0;

    ScoreCounterFixture() { name = "ScoreCounter"; }

    uint32_t anchor_texture_id() override { return LANE::SCORE_NUMBER; }

    void reset(double ms) override {
        score = 0;
        counter.emplace(0, false);
        counter->update(ms, score);
    }
    void on_space(double ms) override {
        score += 1000;
        if (counter) counter->update(ms, score);
    }

    void update(double ms) override { if (counter) counter->update(ms, score); }
    void draw()            override { if (counter) counter->draw(0); }

    std::vector<std::string> debug_lines() override {
        return { "Score : " + std::to_string(score) };
    }
    std::string controls() override { return "SPACE/L-CLICK: +1000 pts  R/R-CLICK: reset"; }
};

// ─── Roll / balloon counters ──────────────────────────────────────────────────

struct DrumrollCounterFixture : public SandboxScreen::Fixture {
    std::optional<DrumrollCounter> counter;
    int count = 0;

    DrumrollCounterFixture() { name = "DrumrollCounter"; }

    uint32_t anchor_texture_id() override { return DRUMROLL_COUNTER::BUBBLE; }

    void reset(double ms) override {
        count = 0;
        counter.emplace();
        counter->update(ms, 0);
    }
    void on_space(double ms) override {
        if (counter) counter->update(ms, ++count);
    }

    void update(double ms) override { if (counter) counter->update(ms, count); }
    void draw()            override { if (counter) counter->draw(0); }

    std::vector<std::string> debug_lines() override {
        return {
            "Count    : " + std::to_string(count),
            std::string("Finished : ") + (counter && counter->is_finished() ? "yes" : "no"),
        };
    }
    std::string controls() override { return "SPACE/L-CLICK: increment  R/R-CLICK: reset"; }
};

struct BalloonCounterFixture : public SandboxScreen::Fixture {
    int  preset_idx = 0;
    int  hit_count  = 0;  // counts up from 0; pops when == total
    std::optional<BalloonCounter> counter;

    static constexpr int totals[] = { 5, 10, 20, 50 };

    BalloonCounterFixture() { name = "BalloonCounter"; }

    uint32_t anchor_texture_id() override { return BALLOON::BUBBLE; }

    void reset(double ms) override {
        hit_count = 0;
        counter.reset();
        counter.emplace(totals[preset_idx], false);
        counter->update(ms, 0);
    }
    void on_space(double ms) override {
        if (!counter || counter->is_finished() || hit_count >= totals[preset_idx]) return;
        counter->update(ms, ++hit_count);
    }

    void update(double ms) override { if (counter) counter->update(ms, hit_count); }
    void draw()            override { if (counter) counter->draw(0); }

    std::vector<std::string> type_names() override { return {"5 hits", "10 hits", "20 hits", "50 hits"}; }
    int  get_type()           override { return preset_idx; }
    void set_type(int idx, double ms) override { preset_idx = idx; reset(ms); }

    std::vector<std::string> debug_lines() override {
        int total = totals[preset_idx];
        return {
            "Hits      : " + std::to_string(hit_count) + " / " + std::to_string(total),
            "Popped    : " + std::string(counter && counter->is_finished() ? "yes" : "no"),
        };
    }
    std::string controls() override { return "SPACE/L-CLICK: hit  R/R-CLICK: reset"; }
};

// ─── Announcements ────────────────────────────────────────────────────────────

struct ComboAnnounceFixture : public SandboxScreen::Fixture {
    int type_idx = 0;
    std::optional<ComboAnnounce> active;

    static constexpr int combos[] = { 100, 200, 500, 1000 };

    ComboAnnounceFixture() { name = "ComboAnnounce"; }

    uint32_t anchor_texture_id() override { return COMBO::COMBO; }

    void reset(double) override { active.reset(); type_idx = 0; }
    void on_space(double ms) override {
        active.emplace(combos[type_idx], ms, PlayerNum::P1);
    }

    void update(double ms) override {
        if (active && active->is_finished) active.reset();
        if (active) active->update(ms);
    }
    void draw() override { if (active) active->draw(0); }

    std::vector<std::string> type_names() override { return {"100", "200", "500", "1000"}; }
    int  get_type()           override { return type_idx; }
    void set_type(int idx, double) override { type_idx = idx; }

    std::vector<std::string> debug_lines() override {
        return { std::string("Active : ") + (active.has_value() ? "yes" : "no") };
    }
    std::string controls() override { return "SPACE/L-CLICK: trigger  R/R-CLICK: reset"; }
};

// ─── Ending animations ────────────────────────────────────────────────────────

struct FCAnimationFixture : public SandboxScreen::Fixture {
    std::optional<FCAnimation> active;

    FCAnimationFixture() { name = "FCAnimation"; }

    uint32_t anchor_texture_id() override { return ENDING_ANIM::FULL_COMBO; }

    void reset(double) override { active.reset(); }
    void on_space(double) override { active.emplace(false); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw()            override { if (active) active->draw(); }

    std::vector<std::string> debug_lines() override {
        return { std::string("Active : ") + (active.has_value() ? "yes" : "no") };
    }
    std::string controls() override { return "SPACE/L-CLICK: trigger  R/R-CLICK: reset"; }
};

struct ClearAnimationFixture : public SandboxScreen::Fixture {
    std::optional<ClearAnimation> active;

    ClearAnimationFixture() { name = "ClearAnimation"; }

    uint32_t anchor_texture_id() override { return ENDING_ANIM::CLEAR; }

    void reset(double) override { active.reset(); }
    void on_space(double) override { active.emplace(false); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw()            override { if (active) active->draw(); }

    std::vector<std::string> debug_lines() override {
        return { std::string("Active : ") + (active.has_value() ? "yes" : "no") };
    }
    std::string controls() override { return "SPACE/L-CLICK: trigger  R/R-CLICK: reset"; }
};

struct FailAnimationFixture : public SandboxScreen::Fixture {
    std::optional<FailAnimation> active;

    FailAnimationFixture() { name = "FailAnimation"; }

    uint32_t anchor_texture_id() override { return ENDING_ANIM::FAIL; }

    void reset(double) override { active.reset(); }
    void on_space(double) override { active.emplace(false); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw()            override { if (active) active->draw(); }

    std::vector<std::string> debug_lines() override {
        return { std::string("Active : ") + (active.has_value() ? "yes" : "no") };
    }
    std::string controls() override { return "SPACE/L-CLICK: trigger  R/R-CLICK: reset"; }
};

struct FireworksFixture : public SandboxScreen::Fixture {
    std::optional<Fireworks> active;

    FireworksFixture() { name = "Fireworks"; }

    uint32_t anchor_texture_id() override { return GOGO_TIME::EXPLOSION; }

    void reset(double) override { active.reset(); }
    void on_space(double) override { active.emplace(); }

    void update(double ms) override {
        if (active && active->is_finished()) active.reset();
        if (active) active->update(ms);
    }
    void draw() override { if (active) active->draw(); }

    std::vector<std::string> debug_lines() override {
        return { std::string("Active : ") + (active.has_value() ? "yes" : "no") };
    }
    std::string controls() override { return "SPACE/L-CLICK: trigger  R/R-CLICK: reset"; }
};
