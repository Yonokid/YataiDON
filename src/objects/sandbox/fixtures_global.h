#include "../../scenes/sandbox.h"
#include "../global/allnet_indicator.h"
#include "../global/chara_3d.h"
#include "../global/coin_overlay.h"
#include "../global/entry_overlay.h"
#include "../global/indicator.h"
#include "../global/nameplate.h"
#include "../global/timer.h"
#include "../../libs/global_data.h"
#include "texture_ids_generated.h"

constexpr int CENTER_X = 220;
constexpr int CENTER_Y = 220;

struct Chara3DFixture : public SandboxScreen::Fixture {
    std::optional<Chara3D> active;
    std::vector<std::string> model_names;
    int model_idx = 0;
    int anim_idx  = 0;

    Chara3DFixture() {
        name = "Chara3D";
        screen = "global";
        fs::path models_dir = fs::path("Skins") / global_data.config->paths.skin / "Models/cos";
        if (fs::exists(models_dir)) {
            for (auto& entry : fs::directory_iterator(models_dir)) {
                if (entry.path().extension() == ".glb")
                    model_names.push_back(entry.path().stem().string());
            }
            std::sort(model_names.begin(), model_names.end(), [](const std::string& a, const std::string& b) {
                char* ae; char* be;
                long ai = std::strtol(a.c_str(), &ae, 10);
                long bi = std::strtol(b.c_str(), &be, 10);
                if (ae != a.c_str() && be != b.c_str()) return ai < bi;
                return a < b;
            });
        }
        if (model_names.empty()) model_names.push_back("0");
    }

    uint32_t anchor_texture_id() override { return 0; }

    void reset(double) override {
        active.emplace(model_names[model_idx]);
        active->set_don_colors({104, 191, 192, 255}, {249, 71, 40, 255}, {249, 240, 225, 255});
        active->set_anim(static_cast<AnimIndex>(anim_idx));
    }

    void on_space(double ms) override { reset(ms); }

    void on_tab(double) override {
        if (!active) return;
        anim_idx = (anim_idx + 1) % active->get_anim_count();
        active->set_anim(static_cast<AnimIndex>(anim_idx));
    }

    void update(double ms) override { if (active) active->update(ms); }

    void draw() override {
        if (!active) return;
        constexpr int PANEL_W = 220;
        float cx = PANEL_W + (tex.screen_width - PANEL_W) / 2.0f;
        float cy = tex.screen_height / 2.0f;
        active->draw(cx, cy);
    }

    std::vector<std::string> type_names() override { return model_names; }
    int  get_type() override { return model_idx; }
    void set_type(int idx, double ms) override { model_idx = idx; reset(ms); }

    std::vector<std::string> debug_lines() override {
        std::vector<std::string> lines;
        lines.push_back("model: " + model_names[model_idx]);
        if (active)
            lines.push_back("anim: " + std::to_string(anim_idx) +
                            " / " + std::to_string(active->get_anim_count() - 1) + " | " + active->get_anim_name(anim_idx));
        return lines;
    }

    std::string controls() override {
        return "VARIANT: next anim  type list: model  R/R-CLICK: reset";
    }
};

struct AllNetIconFixture : public SandboxScreen::Fixture {
    std::optional<AllNetIcon> active;

    AllNetIconFixture() { name = "AllNetIcon"; screen = "global"; }

    uint32_t anchor_texture_id() override { return OVERLAY::ALLNET_INDICATOR; }

    void reset(double)    override { active.emplace(); }
    void on_space(double) override { active.emplace(); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw()            override { if (active) active->draw(0, 0); }

    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: restart"; }
};

struct CoinOverlayFixture : public SandboxScreen::Fixture {
    std::optional<CoinOverlay> active;

    CoinOverlayFixture() { name = "CoinOverlay"; screen = "global"; }

    uint32_t anchor_texture_id() override { return OVERLAY::CAMERA; }

    void reset(double)    override { active.emplace(); }
    void on_space(double) override { active.emplace(); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw()            override { if (active) active->draw(0, 0); }

    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: restart"; }
};

struct EntryOverlayFixture : public SandboxScreen::Fixture {
    std::optional<EntryOverlay> active;

    EntryOverlayFixture() { name = "EntryOverlay"; screen = "global"; }

    uint32_t anchor_texture_id() override { return OVERLAY::BANAPASS_CARD; }

    void reset(double)    override { active.emplace(); }
    void on_space(double) override { active.emplace(); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw()            override { if (active) active->draw(0, 0); }

    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: restart"; }
};

struct IndicatorFixture : public SandboxScreen::Fixture {
    std::optional<Indicator> active;
    int type_idx = 0;

    static constexpr Indicator::State states[] = {
        Indicator::State::SKIP,
        Indicator::State::SIDE,
        Indicator::State::SELECT,
        Indicator::State::WAIT,
    };

    IndicatorFixture() { name = "Indicator"; screen = "global"; }

    uint32_t anchor_texture_id() override { return INDICATOR::BACKGROUND; }

    void reset(double) override { active.emplace(states[type_idx]); }
    void on_space(double) override { active.emplace(states[type_idx]); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw()            override { if (active) active->draw(CENTER_X, CENTER_Y); }

    std::vector<std::string> type_names() override { return {"SKIP", "SIDE", "SELECT", "WAIT"}; }
    int  get_type() override { return type_idx; }
    void set_type(int idx, double ms) override { type_idx = idx; reset(ms); }

    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: reset"; }
};

struct NameplateFixture : public SandboxScreen::Fixture {
    std::optional<Nameplate> active;
    int type_idx = 0;

    NameplateFixture() { name = "Nameplate"; screen = "global"; }

    uint32_t anchor_texture_id() override { return NAMEPLATE::OUTLINE; }

    void reset(double) override {
        switch (type_idx) {
            case 0: active.emplace("Test Player", "Test Title", PlayerNum::P1, -1,   false, false, 0); break;
            case 1: active.emplace("Test Player", "Test Title", PlayerNum::P1,  5,   true,  false, 1); break;
            case 2: active.emplace("Test Player", "Test Title", PlayerNum::P1,  5,   false, true,  0); break;
            case 3: active.emplace("Test Player", "Test Title", PlayerNum::AI, -1,   false, false, 0); break;
        }
    }
    void on_space(double ms) override { reset(ms); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw()            override { if (active) active->draw(CENTER_X, CENTER_Y); }

    std::vector<std::string> type_names() override { return {"NORMAL", "GOLD DAN", "RAINBOW", "AI"}; }
    int  get_type() override { return type_idx; }
    void set_type(int idx, double ms) override { type_idx = idx; reset(ms); }

    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: reset"; }
};

struct TimerFixture : public SandboxScreen::Fixture {
    std::optional<Timer> active;
    int type_idx = 0;

    static constexpr int times[] = { 5, 30, 60, 120 };

    TimerFixture() { name = "Timer"; screen = "global"; }

    uint32_t anchor_texture_id() override { return TIMER::BG; }

    void reset(double ms) override { active.emplace(times[type_idx], ms, []{}); }
    void on_space(double ms) override { active.emplace(times[type_idx], ms, []{}); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw()            override { if (active) active->draw(0, 0); }

    std::vector<std::string> type_names() override { return {"5s", "30s", "60s", "120s"}; }
    int  get_type() override { return type_idx; }
    void set_type(int idx, double ms) override { type_idx = idx; reset(ms); }

    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: reset"; }
};
