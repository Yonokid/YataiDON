#include "sandbox.h"
#include "../objects/sandbox/fixtures.h"

static constexpr int SB_PANEL_W  = 220;
static constexpr int SB_ITEM_H   = 32;
static constexpr int SB_BTN_H    = 28;
static constexpr int SB_BTN_MARG = 8;
static constexpr int SB_TYPE_H   = 24;

void SandboxScreen::on_screen_start() {
    ray::ShowCursor();
    try {
        tex.load_screen_textures("game");
        audio->load_screen_sounds("game");
    } catch (const std::exception& e) {
        spdlog::warn("Sandbox: could not load game assets: {}", e.what());
    }
    spdlog::info("Sandbox screen initialized");

    fixtures.clear();
    fixtures.push_back(std::make_unique<JudgmentFixture>());
    fixtures.push_back(std::make_unique<DrumHitFixture>());
    fixtures.push_back(std::make_unique<ComboFixture>());
    fixtures.push_back(std::make_unique<LaneHitEffectFixture>());
    fixtures.push_back(std::make_unique<GaugeHitEffectFixture>());
    fixtures.push_back(std::make_unique<GogoTimeFixture>());
    fixtures.push_back(std::make_unique<ScoreCounterFixture>());
    fixtures.push_back(std::make_unique<DrumrollCounterFixture>());
    fixtures.push_back(std::make_unique<BalloonCounterFixture>());
    fixtures.push_back(std::make_unique<ComboAnnounceFixture>());
    fixtures.push_back(std::make_unique<FCAnimationFixture>());
    fixtures.push_back(std::make_unique<ClearAnimationFixture>());
    fixtures.push_back(std::make_unique<FailAnimationFixture>());
    fixtures.push_back(std::make_unique<FireworksFixture>());
    fixtures.push_back(std::make_unique<BranchIndicatorFixture>());
    fixtures.push_back(std::make_unique<GaugeFixture>());
    fixtures.push_back(std::make_unique<JudgeCounterFixture>());
    fixtures.push_back(std::make_unique<KusudamaCounterFixture>());
    fixtures.push_back(std::make_unique<NoteArcFixture>());
    fixtures.push_back(std::make_unique<ResultTransitionFixture>());
    fixtures.push_back(std::make_unique<TransitionFixture>());
    fixtures.push_back(std::make_unique<ScoreCounterAnimFixture>());
    fixtures.push_back(std::make_unique<SongInfoFixture>());

    current_ms = fixture_start_ms = get_current_ms();
    for (auto& f : fixtures) f->reset(current_ms);
    fixture_idx  = 0;
    panel_scroll = 0;
}

Screens SandboxScreen::on_screen_end(Screens next_screen) {
    fixtures.clear();
    ray::HideCursor();
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> SandboxScreen::handle_input() {
    if (ray::IsKeyPressed(ray::KEY_ESCAPE))
        return on_screen_end(Screens::TITLE);

    if (fixtures.empty()) return std::nullopt;

    ray::Vector2 mouse = ray::GetMousePosition();
    bool lclick        = ray::IsMouseButtonPressed(ray::MOUSE_BUTTON_LEFT);
    bool rclick        = ray::IsMouseButtonPressed(ray::MOUSE_BUTTON_RIGHT);

    if (ray::IsKeyPressed(ray::KEY_SPACE)) {
        current_ms = get_current_ms();
        fixtures[fixture_idx]->on_space(current_ms);
    }
    if (ray::IsKeyPressed(ray::KEY_R)) {
        current_ms = fixture_start_ms = get_current_ms();
        fixtures[fixture_idx]->reset(current_ms);
    }

    // Fixed bottom layout (computed from bottom up)
    const int btn_y_pause   = tex.screen_height - 16 - 4 - SB_BTN_H;
    const int var_btn_y     = btn_y_pause - 4 - SB_BTN_H;
    auto panel_type_list    = fixtures[fixture_idx]->type_names();
    const int type_row_h    = SB_TYPE_H + 2;
    const int n_types       = (int)panel_type_list.size();
    const int y_above_var   = var_btn_y - 8 - n_types * type_row_h - (n_types ? 8 : 0);
    const int y_types_start = y_above_var + 8;
    const int scroll_bottom = y_above_var;  // scrollable area ends here
    const int scroll_area_h = scroll_bottom - 46;
    const int content_h     = (int)fixtures.size() * SB_ITEM_H;
    const int max_scroll     = std::max(0, content_h - scroll_area_h);

    // Mouse-wheel scroll over panel
    if (mouse.x < SB_PANEL_W) {
        float wheel = ray::GetMouseWheelMove();
        if (wheel != 0.0f)
            panel_scroll = std::max(0, std::min(panel_scroll - (int)(wheel * SB_ITEM_H), max_scroll));
    }

    if (lclick && mouse.x < SB_PANEL_W) {
        // Pause button (fixed)
        if (mouse.x >= SB_BTN_MARG && mouse.x < SB_PANEL_W - SB_BTN_MARG &&
            mouse.y >= btn_y_pause && mouse.y < btn_y_pause + SB_BTN_H)
            paused = !paused;

        // Variant button (fixed)
        if (mouse.x >= SB_BTN_MARG && mouse.x < SB_PANEL_W - SB_BTN_MARG &&
            mouse.y >= var_btn_y && mouse.y < var_btn_y + SB_BTN_H)
            fixtures[fixture_idx]->on_tab(current_ms);

        // Type buttons (fixed)
        for (int i = 0; i < n_types; ++i) {
            int ty = y_types_start + i * type_row_h;
            if (mouse.y >= ty && mouse.y < ty + SB_TYPE_H) {
                fixtures[fixture_idx]->set_type(i, current_ms);
                break;
            }
        }

        // Fixture list (scrollable) — convert screen y → content y
        if (mouse.y >= 46 && mouse.y < scroll_bottom) {
            int cy = (int)mouse.y - 46 + panel_scroll;
            for (int i = 0; i < (int)fixtures.size(); ++i) {
                if (cy >= i * SB_ITEM_H && cy < (i + 1) * SB_ITEM_H) {
                    if (i != fixture_idx) {
                        fixture_idx = i;
                        current_ms  = fixture_start_ms = get_current_ms();
                        fixtures[fixture_idx]->reset(current_ms);
                    }
                    break;
                }
            }
        }
    }

    if (mouse.x >= SB_PANEL_W) {
        if (lclick) {
            current_ms = get_current_ms();
            fixtures[fixture_idx]->on_space(current_ms);
        }
        if (rclick) {
            current_ms = fixture_start_ms = get_current_ms();
            fixtures[fixture_idx]->reset(current_ms);
        }
    }

    if (ray::IsKeyPressed(ray::KEY_COMMA)) {
        current_ms -= FRAME_MS;
        if (current_ms < fixture_start_ms) current_ms = fixture_start_ms;
        fixtures[fixture_idx]->update(current_ms);
    } else if (ray::IsKeyPressed(ray::KEY_PERIOD)) {
        current_ms += FRAME_MS;
        fixtures[fixture_idx]->update(current_ms);
    }

    return std::nullopt;
}

std::optional<Screens> SandboxScreen::update() {
    Screen::update();

    auto nav = handle_input();
    if (nav.has_value()) return nav;

    if (!fixtures.empty() && !paused) {
        current_ms = get_current_ms();
        fixtures[fixture_idx]->update(current_ms);
    }
    return std::nullopt;
}

void SandboxScreen::draw_panel() const {
    ray::DrawRectangle(0, 0, SB_PANEL_W, tex.screen_height, ray::Color{20, 20, 30, 230});
    ray::DrawLine(SB_PANEL_W, 0, SB_PANEL_W, tex.screen_height, ray::Color{80, 80, 100, 255});

    ray::Vector2 mouse = ray::GetMousePosition();

    // Fixed header
    ray::DrawText("SANDBOX", 10, 10, 20, ray::WHITE);
    ray::DrawLine(0, 36, SB_PANEL_W, 36, ray::Color{80, 80, 100, 255});

    // Fixed bottom layout (same formula as handle_input)
    const int btn_y_pause   = tex.screen_height - 16 - 4 - SB_BTN_H;
    const int var_btn_y     = btn_y_pause - 4 - SB_BTN_H;
    auto type_list          = fixtures[fixture_idx]->type_names();
    const int type_row_h    = SB_TYPE_H + 2;
    const int n_types       = (int)type_list.size();
    const int y_above_var   = var_btn_y - 8 - n_types * type_row_h - (n_types ? 8 : 0);
    const int y_types_start = y_above_var + 8;
    const int scroll_bottom = y_above_var;

    // ── Scrollable fixture list ───────────────────────────────────────────────
    constexpr int SCROLL_TOP = 46;
    const int scroll_area_h  = scroll_bottom - SCROLL_TOP;
    const int content_h      = (int)fixtures.size() * SB_ITEM_H;
    const int max_scroll     = std::max(0, content_h - scroll_area_h);

    ray::BeginScissorMode(0, SCROLL_TOP, SB_PANEL_W, scroll_area_h);
    for (int i = 0; i < (int)fixtures.size(); ++i) {
        int iy = SCROLL_TOP + i * SB_ITEM_H - panel_scroll;
        bool selected = (i == fixture_idx);
        bool hovered  = !selected && mouse.x < SB_PANEL_W && mouse.y >= iy && mouse.y < iy + SB_ITEM_H;
        if (selected)
            ray::DrawRectangle(0, iy, SB_PANEL_W, SB_ITEM_H, ray::Color{60, 80, 180, 220});
        else if (hovered)
            ray::DrawRectangle(0, iy, SB_PANEL_W, SB_ITEM_H, ray::Color{40, 50, 100, 160});
        ray::Color col = selected ? ray::WHITE : (hovered ? ray::Color{220, 220, 255, 255} : ray::Color{180, 180, 180, 255});
        ray::DrawText(fixtures[i]->name.c_str(), 10, iy + 7, 16, col);
    }
    ray::EndScissorMode();

    if (max_scroll > 0) {
        float ratio   = (float)scroll_area_h / content_h;
        int   thumb_h = std::max(16, (int)(scroll_area_h * ratio));
        int   thumb_y = SCROLL_TOP + (int)((scroll_area_h - thumb_h) * (float)panel_scroll / max_scroll);
        ray::DrawRectangle(SB_PANEL_W - 3, thumb_y, 2, thumb_h, ray::Color{120, 130, 160, 180});
    }

    // ── Fixed bottom: separator → types → variant → pause ────────────────────
    ray::DrawLine(0, scroll_bottom + 4, SB_PANEL_W, scroll_bottom + 4, ray::Color{80, 80, 100, 255});

    // Type buttons
    if (n_types > 0) {
        for (int i = 0; i < n_types; ++i) {
            int ty  = y_types_start + i * type_row_h;
            bool sel = (i == fixtures[fixture_idx]->get_type());
            bool hov = !sel && mouse.x < SB_PANEL_W && mouse.y >= ty && mouse.y < ty + SB_TYPE_H;
            if (sel)
                ray::DrawRectangle(SB_BTN_MARG, ty, SB_PANEL_W - SB_BTN_MARG * 2, SB_TYPE_H,
                                   ray::Color{60, 80, 180, 220});
            else if (hov)
                ray::DrawRectangle(SB_BTN_MARG, ty, SB_PANEL_W - SB_BTN_MARG * 2, SB_TYPE_H,
                                   ray::Color{40, 50, 100, 160});
            ray::Color tcol = sel ? ray::WHITE : (hov ? ray::Color{220, 220, 255, 255} : ray::Color{180, 180, 180, 255});
            ray::DrawText(type_list[i].c_str(), SB_BTN_MARG + 4, ty + (SB_TYPE_H - 12) / 2, 12, tcol);
        }
        int y_types_end = y_types_start + n_types * type_row_h;
        ray::DrawLine(0, y_types_end + 4, SB_PANEL_W, y_types_end + 4, ray::Color{80, 80, 100, 255});
    }

    // Variant button
    bool var_hov = mouse.x >= SB_BTN_MARG && mouse.x < SB_PANEL_W - SB_BTN_MARG &&
                   mouse.y >= var_btn_y && mouse.y < var_btn_y + SB_BTN_H;
    ray::DrawRectangle(SB_BTN_MARG, var_btn_y, SB_PANEL_W - SB_BTN_MARG * 2, SB_BTN_H,
                       ray::Color{80, 80, 160, static_cast<unsigned char>(var_hov ? 255 : 200)});
    ray::DrawText("VARIANT", SB_BTN_MARG + 6, var_btn_y + 7, 14, ray::WHITE);

    // Pause button
    bool btn_hov = mouse.x >= SB_BTN_MARG && mouse.x < SB_PANEL_W - SB_BTN_MARG &&
                   mouse.y >= btn_y_pause && mouse.y < btn_y_pause + SB_BTN_H;
    ray::Color btn_col = paused
        ? ray::Color{160, 50,  50,  static_cast<unsigned char>(btn_hov ? 255 : 210)}
        : ray::Color{50,  130, 50,  static_cast<unsigned char>(btn_hov ? 255 : 210)};
    ray::DrawRectangle(SB_BTN_MARG, btn_y_pause, SB_PANEL_W - SB_BTN_MARG * 2, SB_BTN_H, btn_col);
    ray::DrawText(paused ? "|| PAUSED" : ">  PLAYING", SB_BTN_MARG + 6, btn_y_pause + 7, 14, ray::WHITE);

    ray::DrawText("ESC: title", 6, tex.screen_height - 14, 11, ray::Color{120, 120, 130, 255});
}

void SandboxScreen::draw_debug() const {
    if (fixtures.empty()) return;
    const auto& f = fixtures[fixture_idx];

    int x = SB_PANEL_W + 8;
    int y = 8;

    if (paused) {
        ray::DrawText("[PAUSED]", x, y, 14, ray::Color{255, 220, 60, 255});
        x += ray::MeasureText("[PAUSED]", 14) + 8;
    }
    std::string t_str = "t = " + std::to_string((int)(current_ms - fixture_start_ms)) + " ms";
    ray::DrawText(t_str.c_str(), x, y, 14, ray::Color{180, 180, 180, 255});
    x = SB_PANEL_W + 8;
    y += 20;

    for (const auto& line : f->debug_lines()) {
        ray::DrawText(line.c_str(), x, y, 14, ray::Color{180, 210, 255, 255});
        y += 18;
    }

    if (paused)
        ray::DrawText(",/.: step frame", SB_PANEL_W + 8, tex.screen_height - 32, 11,
                      ray::Color{255, 220, 60, 200});
    ray::DrawText(f->controls().c_str(), SB_PANEL_W + 8, tex.screen_height - 18, 12,
                  ray::Color{120, 130, 150, 255});
}

void SandboxScreen::draw() {
    constexpr int TILE = 32;
    for (int ty = 0; ty < tex.screen_height; ty += TILE) {
        for (int tx = 0; tx < tex.screen_width; tx += TILE) {
            ray::Color bg = ((tx / TILE + ty / TILE) % 2 == 0)
                ? ray::Color{10, 10, 20, 255}
                : ray::Color{18, 18, 30, 255};
            ray::DrawRectangle(tx, ty, TILE, TILE, bg);
        }
    }

    if (!fixtures.empty()) {
        auto& f = fixtures[fixture_idx];
        uint32_t anchor = f->anchor_texture_id();

        // Shift the entire game coordinate space so the anchor texture lands at the viewport center
        const float vcx = SB_PANEL_W + (tex.screen_width  - SB_PANEL_W) / 2.0f;
        const float vcy =               tex.screen_height / 2.0f;
        auto it = tex.textures.find(anchor);
        if (it != tex.textures.end()) {
            tex.draw_offset_x = vcx - (float)it->second->x[0] - (float)it->second->width / 2.0f;
            tex.draw_offset_y = vcy - (float)it->second->y[0] - (float)it->second->height / 2.0f;
        }

        f->draw();

        tex.draw_offset_x = 0.0f;
        tex.draw_offset_y = 0.0f;
    }

    draw_panel();
    draw_debug();
}
