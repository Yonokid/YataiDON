#include "../../scenes/sandbox.h"
#include "../result/background.h"
#include "../result/result_crown.h"
#include "../result/fade_in.h"
#include "../result/bottom_characters.h"
#include "../result/high_score_indicator.h"
#include "../enums.h"
#include "texture_ids_generated.h"

struct ResultBackgroundFixture : public SandboxScreen::Fixture {
    std::optional<ResultBackground> active;

    ResultBackgroundFixture() { name = "ResultBackground"; screen = "result"; }

    uint32_t anchor_texture_id() override { return BACKGROUND::RESULT_TEXT; }

    void reset(double) override { active.emplace(PlayerNum::P1, (float)tex.screen_width); }
    void on_space(double) override { active.emplace(PlayerNum::P1, (float)tex.screen_width); }

    void update(double) override {}
    void draw() override { if (active) active->draw(); }

    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: restart"; }
};

struct ResultCrownFixture : public SandboxScreen::Fixture {
    std::optional<ResultCrown> active;
    int type_idx = 0;

    static constexpr CrownType crown_types[] = { CROWN_CLEAR, CROWN_FC, CROWN_DFC };

    ResultCrownFixture() { name = "ResultCrown"; screen = "result"; }

    uint32_t anchor_texture_id() override { return CROWN::CROWN_CLEAR; }

    void reset(double) override { active.emplace((int)crown_types[type_idx], false); }
    void on_space(double) override { active.emplace((int)crown_types[type_idx], false); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw() override { if (active) active->draw(); }

    std::vector<std::string> type_names() override { return {"CLEAR", "FC", "DFC"}; }
    int  get_type() override { return type_idx; }
    void set_type(int idx, double ms) override { type_idx = idx; reset(ms); }

    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: reset"; }
};

struct ResultFadeInFixture : public SandboxScreen::Fixture {
    std::optional<FadeIn> active;

    ResultFadeInFixture() { name = "ResultFadeIn"; screen = "result"; }

    uint32_t anchor_texture_id() override { return BACKGROUND::BACKGROUND_1P; }

    void reset(double) override { active.emplace(PlayerNum::P1); }
    void on_space(double) override { active.emplace(PlayerNum::P1); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw() override { if (active) active->draw(); }

    std::vector<std::string> debug_lines() override {
        return { std::string("Finished : ") + (active && active->is_finished() ? "yes" : "no") };
    }
    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: restart"; }
};

struct BottomCharactersFixture : public SandboxScreen::Fixture {
    std::optional<BottomCharacters> active;
    int type_idx = 0;

    static constexpr ResultState states[] = { ResultState::FAIL, ResultState::CLEAR, ResultState::RAINBOW };

    BottomCharactersFixture() { name = "BottomCharacters"; screen = "result"; }

    uint32_t anchor_texture_id() override { return BOTTOM::CHARA_CENTER; }

    void reset(double) override {
        type_idx = 0;
        active.emplace();
        active->start();
    }
    void on_space(double) override {
        active.emplace();
        active->start();
    }

    void update(double ms) override { if (active) active->update(ms, (int)states[type_idx]); }
    void draw()         override { if (active) active->draw(); }

    std::vector<std::string> type_names() override { return {"FAIL", "CLEAR", "RAINBOW"}; }
    int  get_type() override { return type_idx; }
    void set_type(int idx, double) override { type_idx = idx; }

    std::vector<std::string> debug_lines() override {
        return { std::string("Finished : ") + (active && active->is_finished() ? "yes" : "no") };
    }
    std::string controls() override { return "SPACE/L-CLICK: restart  VARIANT: cycle state  R/R-CLICK: reset"; }
};

struct HighScoreIndicatorFixture : public SandboxScreen::Fixture {
    std::optional<HighScoreIndicator> active;
    int type_idx = 0;

    static constexpr int diffs[] = { 500, 1000, 5000, 10000 };

    HighScoreIndicatorFixture() { name = "HighScoreIndicator"; screen = "result"; }

    uint32_t anchor_texture_id() override { return SCORE::HIGH_SCORE_JA; }

    void reset(double) override { type_idx = 0; active.emplace(0, diffs[type_idx], false); }
    void on_space(double) override { active.emplace(0, diffs[type_idx], false); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw()            override { if (active) active->draw(); }

    std::vector<std::string> type_names() override { return {"+500", "+1000", "+5000", "+10000"}; }
    int  get_type() override { return type_idx; }
    void set_type(int idx, double ms) override { type_idx = idx; active.emplace(0, diffs[type_idx], false); (void)ms; }

    std::string controls() override { return "SPACE/L-CLICK: trigger  R/R-CLICK: reset"; }
};
