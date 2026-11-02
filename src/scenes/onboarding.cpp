#include "onboarding.h"

#include <algorithm>

#include "../libs/file/filesystem.h"
#include "../libs/gfx/text.h"
#include "../libs/global_data.h"
#include "../libs/input/input.h"
#include "../libs/network/skin_updater.h"

namespace {
    struct SkinEntry {
        const char* name;
        const char* url;
        const char* parent; // "" for none
    };
    constexpr SkinEntry kSkins[] = {
        {"PyTaikoGreen", "https://ese.tjadataba.se/Yonokid/PyTaikoGreen", ""},
        {"YataiDON14", "https://ese.tjadataba.se/GanYiZhong/YataiDON14", "PyTaikoGreen"},
        {"YataiDON8", "https://ese.tjadataba.se/GanYiZhong/YataiDON8", "YataiDON14"},
        {"YataiDON12Asia", "https://ese.tjadataba.se/GanYiZhong/YataiDON12Asia", "YataiDON14"},
        {"YataiDONNijiiro", "https://ese.tjadataba.se/Yonokid/YataiDONNijiiro", "PyTaikoGreen"},
        {"YataiDONRed", "https://ese.tjadataba.se/Yonokid/YataiDONRed", "PyTaikoGreen"},
    };
    constexpr int kSkinCount = static_cast<int>(std::size(kSkins));

    int index_of(const std::string& name) {
        for (int i = 0; i < kSkinCount; i++) {
            if (name == kSkins[i].name) {
                return i;
            }
        }
        return -1;
    }

    int parent_of(int skin) {
        return index_of(kSkins[skin].parent);
    }

    bool has_ancestor(int skin, int ancestor) {
        for (int p = parent_of(skin); p >= 0; p = parent_of(p)) {
            if (p == ancestor) {
                return true;
            }
        }
        return false;
    }
} // namespace

int OnboardingScreen::skin_count() const {
    return kSkinCount;
}

int OnboardingScreen::required_by(int skin) const {
    for (int i = 0; i < kSkinCount; i++) {
        if (selected[i] && has_ancestor(i, skin)) {
            return i;
        }
    }
    return -1;
}

void OnboardingScreen::on_screen_start() {
    // no Screen::on_screen_start(): there are no textures/sounds for this screen
    selected.assign(kSkinCount, false);
    installed.assign(kSkinCount, false);
    for (int i = 0; i < kSkinCount; i++) {
        installed[i] = fs::exists(fs::path("Skins") / kSkins[i].name / "Graphics");
    }
    const int current = index_of(global_data.config->paths.skin.string());
    if (current >= 0) {
        toggle_skin(current);
    }
    online = global_data.config->network.online_play;
    row = 0;
    downloading = download_started = false;
    skinless_start = global_data.skinless;
    error.clear();
    screen_init = true;
}

void OnboardingScreen::toggle_skin(int skin) {
    error.clear();
    if (selected[skin]) {
        const int child = required_by(skin);
        if (child >= 0) {
            error = std::string(kSkins[skin].name) + " is required by " + kSkins[child].name;
            return;
        }
        selected[skin] = false;
        return;
    }
    for (int s = skin; s >= 0; s = parent_of(s)) {
        selected[s] = true;
    }
}

void OnboardingScreen::begin_download() {
    error.clear();
    if (std::ranges::none_of(selected, [](bool b) { return b; })) {
        error = "Select at least one skin";
        return;
    }
    if (!SkinUpdater::available()) {
        for (int i = 0; i < kSkinCount; ++i) {
            if (selected[i] && !installed[i]) {
                error = "Install selected skins in " + fs::absolute("Skins").string() + " first";
                return;
            }
        }
    }
    downloading = true;
    download_started = false;
}

// Every selected skin is on disk: apply the choices and load the skin. False (with `error` set)
// if the skin would not load, so the player is never left without one.
bool OnboardingScreen::apply() {
    Config& cfg = *global_data.config;
    const std::string before = cfg.paths.skin.string();
    const bool before_online = cfg.network.online_play;

    // active skin: the one already configured if it is a leaf, else the first leaf
    auto usable = [&](int i) {
        return selected[i] && required_by(i) < 0 && fs::exists(fs::path("Skins") / kSkins[i].name / "Graphics");
    };
    int active = -1;
    for (int i = 0; i < kSkinCount && active < 0; i++) {
        if (usable(i)) {
            active = i;
        }
    }
    const int current = index_of(before);
    if (current >= 0 && usable(current)) {
        active = current;
    }
    cfg.network.online_play = online;
    if (active >= 0) {
        cfg.paths.skin = kSkins[active].name;
    }

    if (global_data.skinless || cfg.paths.skin.string() != before) {
        drop_other_screens_for_skin_reload();
        try {
            load_skin();
        } catch (const std::exception& e) {
            spdlog::error("Onboarding: skin {} failed to load: {}", cfg.paths.skin.string(), e.what());
            cfg.paths.skin = before;
            cfg.network.online_play = before_online;
            error = "Could not load the skin. Try again.";
            return false;
        }
        reload_skin_screens();
    }
    global_data.skinless = false;
    global_data.needs_onboarding = false;
    save_config(cfg);
    return true;
}

Screens OnboardingScreen::on_screen_end(Screens next_screen) {
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> OnboardingScreen::update() {
    if (auto result = Screen::update()) {
        return result;
    }
    if (!screen_init) {
        return std::nullopt;
    }

    if (downloading) { // no way out until the download is done
        if (!download_started) {
            // an updater pass from the loading screen may still be running
            if (skin_updater.in_progress()) {
                return std::nullopt;
            }
            std::vector<SkinUpdater::Request> requests;
            for (int i = 0; i < kSkinCount; i++) {
                if (selected[i] && !installed[i]) {
                    requests.push_back({kSkins[i].name, kSkins[i].url});
                }
            }
            skin_updater.download(std::move(requests));
            download_started = true;
            return std::nullopt;
        }
        const SkinUpdater::Status st = skin_updater.snapshot();
        if (st.running) {
            return std::nullopt;
        }
        downloading = false;
        if (!st.failed.empty()) {
            error = "Download failed: " + st.failed.front() + (st.failed.size() > 1 ? " and more" : "") +
                    ". Check your connection and try again.";
            return std::nullopt;
        }
        if (!apply()) {
            return std::nullopt;
        }
        // started without a skin: the loading screen has not run yet
        return on_screen_end(skinless_start ? Screens::LOADING : Screens::TITLE);
    }

    // Up/Down (arrows or D-pad) move between rows; the drum-only scheme below still works
    const int rows = kSkinCount + 2;
    if (key_pressed(SDL_SCANCODE_UP) || check_key_pressed(10000 + 1)) {
        row = (row + rows - 1) % rows;
        return std::nullopt;
    }
    if (key_pressed(SDL_SCANCODE_DOWN) || check_key_pressed(10000 + 3)) {
        row = (row + 1) % rows;
        return std::nullopt;
    }
    if (global_data.config->keys.gamepad_menu_layout) {
        if (check_key_pressed(global_data.config->keys.back_key)) {
            if (!skinless_start) {
                return on_screen_end(Screens::TITLE);
            }
        } else if (menu_confirm_pressed()) {
            if (row == continue_row()) {
                begin_download();
            } else if (row < kSkinCount) {
                toggle_skin(row);
            } else if (row == online_row()) {
                online = !online;
            }
        }
        return std::nullopt;
    }
    // drum-only: don moves down (starts the download on Continue), kat toggles the current row
    if (menu_confirm_pressed()) {
        if (row == continue_row()) {
            begin_download();
        } else {
            row++;
        }
    } else if (menu_left_pressed() || menu_right_pressed()) {
        if (row < kSkinCount) {
            toggle_skin(row);
        } else if (row == online_row()) {
            online = !online;
        } else {
            row = 0; // kat on Continue: back to the top
        }
    }
    return std::nullopt;
}

void OnboardingScreen::draw_line(const std::string& text, float y, int size, Color color) {
    Font font = font_manager.get_font(text, size);
    Vec2 dim = measure_text_ex(font, text.c_str(), static_cast<float>(size), 1.0f);
    draw_text_ex(font, text.c_str(), Vec2{(tex.screen_width - dim.x) / 2.0f, y}, static_cast<float>(size), 1.0f, color);
}

void OnboardingScreen::draw() {
    draw_rectangle(0, 0, tex.screen_width, tex.screen_height, Color(30, 30, 30, 255));
    const float s = tex.screen_height / 720.0f;
    draw_line("Welcome to YataiDON", 50 * s, static_cast<int>(52 * s), Color::WHITE);

    if (downloading) {
        const SkinUpdater::Status st = skin_updater.snapshot();
        if (!download_started) {
            draw_line("Waiting for the skin updater...", 300 * s, static_cast<int>(34 * s), Color::LIGHTGRAY);
        } else {
            draw_line("Downloading " + (st.current_skin.empty() ? std::string("skins") : st.current_skin) + "...",
                      280 * s, static_cast<int>(34 * s), Color::YELLOW);
            draw_line(st.current_file, 340 * s, static_cast<int>(22 * s), Color::GRAY);
            draw_line(std::to_string(st.files_updated) + " files downloaded", 380 * s, static_cast<int>(22 * s),
                      Color::GRAY);
        }
        draw_line("Please wait", 600 * s, static_cast<int>(22 * s), Color::GRAY);
        return;
    }

    draw_line("Pick your skins", 125 * s, static_cast<int>(26 * s), Color::GRAY);
    for (int i = 0; i < kSkinCount; i++) {
        std::string line = std::string(selected[i] ? "[x] " : "[ ] ") + kSkins[i].name;
        if (installed[i]) {
            line += "  (installed)";
        }
        const int child = selected[i] ? required_by(i) : -1;
        if (child >= 0) {
            line += "  (needed by " + std::string(kSkins[child].name) + ")";
        }
        Color c = i == row ? Color::YELLOW : (selected[i] ? Color::WHITE : Color::LIGHTGRAY);
        draw_line(line, (165 + i * 42) * s, static_cast<int>(28 * s), c);
    }
    draw_line(std::string("Online mode: ") + (online ? "ON" : "OFF"), 440 * s, static_cast<int>(32 * s),
              row == online_row() ? Color::YELLOW : Color::LIGHTGRAY);
    draw_line("Continue", 500 * s, static_cast<int>(32 * s), row == continue_row() ? Color::YELLOW : Color::LIGHTGRAY);
    if (!error.empty()) {
        draw_line(error, 565 * s, static_cast<int>(22 * s), Color::RED);
    }
    draw_line(global_data.config->keys.gamepad_menu_layout ? "D-pad: move    Confirm: toggle / continue"
                                                           : "Don: next / continue    Ka: toggle    Up/Down: move",
              650 * s, static_cast<int>(20 * s), Color::GRAY);
}
