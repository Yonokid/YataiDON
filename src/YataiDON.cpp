#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <rlgl.h>

#include "libs/animation.h"
#include "libs/audio.h"
#include "libs/camera_utils.h"
#include "libs/filesystem.h"
#include "libs/global_data.h"
#include "libs/ime_ui.h"
#include "libs/input.h"
#include "libs/logging.h"
#include "libs/network.h"
#include "libs/profiler.h"
#include "libs/screen.h"
#include "libs/script.h"
#include "libs/skin_updater.h"
#include "libs/song_parser.h"
#include "libs/text.h"
#include "libs/webcam.h"

#ifdef _WIN32
#include "platform/platform_windows.h"
#elif defined(__ANDROID__)
#include "platform/platform_android.h"
#elif defined(__EMSCRIPTEN__)
#include "platform/platform_emscripten.h"
#elif defined(__linux__)
#include "platform/platform_linux.h"
#endif

#ifdef PLATFORM_ANDROID
#include "libs/sdl_main_compat.h"
#endif

#include "objects/global/debug_menu.h"
#include "objects/global/fps_counter.h"
#include "scenes/dan_result.h"
#include "scenes/dan_select.h"
#include "scenes/entry.h"
#include "scenes/game.h"
#include "scenes/game_2p.h"
#include "scenes/game_dan.h"
#include "scenes/game_over.h"
#include "scenes/game_practice.h"
#include "scenes/input_cali.h"
#include "scenes/input_test.h"
#include "scenes/loading.h"
#include "scenes/onboarding.h"
#include "scenes/result.h"
#include "scenes/result_2p.h"
#include "scenes/retry_game.h"
#include "scenes/settings.h"
#include "scenes/song_select.h"
#include "scenes/song_select_2p.h"
#include "scenes/song_select_practice.h"
#include "scenes/title.h"

void draw_outer_border(int screen_width, int screen_height, Color last_color) {
    draw_rectangle(-screen_width, 0, screen_width, screen_height, last_color);
    draw_rectangle(screen_width, 0, screen_width, screen_height, last_color);
    draw_rectangle(0, -screen_height, screen_width, screen_height, last_color);
    draw_rectangle(0, screen_height, screen_width, screen_height, last_color);
    draw_rectangle(-screen_width, -screen_height, screen_width, screen_height, last_color);
    draw_rectangle(screen_width, -screen_height, screen_width, screen_height, last_color);
    draw_rectangle(-screen_width, screen_height, screen_width, screen_height, last_color);
    draw_rectangle(screen_width, screen_height, screen_width, screen_height, last_color);
}

static void draw_skin_update_status() {
    const SkinUpdater::Status st = skin_updater.snapshot();
    if (!st.running) {
        return;
    }

    const int size = static_cast<int>(20.0f * global_tex.screen_scale);
    const float x = static_cast<float>(size);
    float y = static_cast<float>(size) * 2.0f;

    const std::string skin_line =
        st.current_skin.empty() ? "Checking for skin updates..." : "Updating skin: " + st.current_skin;
    ray::Font skin_font = font_manager.get_font(skin_line, size);
    draw_text_ex(skin_font, skin_line.c_str(), Vec2{x, y}, static_cast<float>(size), 1.0f, Color::YELLOW);

    if (!st.current_file.empty()) {
        y += static_cast<float>(size);
        ray::Font file_font = font_manager.get_font(st.current_file, size);
        draw_text_ex(file_font, st.current_file.c_str(), Vec2{x, y}, static_cast<float>(size), 1.0f, Color::WHITE);
    }
}

[[noreturn]] static void exit_now(int code) {
    std::cout.flush();
    std::cerr.flush();
    if (auto logger = spdlog::default_logger()) {
        logger->flush();
    }
    std::_Exit(code);
}

static std::filesystem::path path_from_arg(const std::string& arg) {
#ifdef _WIN32
    return win32_path_from_utf8(arg);
#else
    try {
        return std::filesystem::path(arg);
    } catch (const std::exception&) {
        return {};
    }
#endif
}

// Consumes --card-port/--card-baud/--card-poll-ms (and removes them from argv) so check_args never sees them.
static void parse_card_reader_args(int& argc, char* argv[], CardReaderConfig& cr) {
    int out = 1;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        const bool known = arg == "--card-port" || arg == "--card-baud" || arg == "--card-poll-ms";
        if (!known) {
            argv[out++] = argv[i];
            continue;
        }
        if (i + 1 >= argc) {
            std::cerr << "Error: " << arg << " requires a value\n";
            exit_now(1);
        }
        std::string val = argv[++i];
        if (arg == "--card-port") {
            cr.port = val;
            continue;
        }
        try {
            size_t pos = 0;
            int n = std::stoi(val, &pos);
            if (pos != val.size() || n <= 0) {
                throw std::invalid_argument(val);
            }
            (arg == "--card-baud" ? cr.baudrate : cr.poll_interval_ms) = n;
        } catch (const std::exception&) {
            std::cerr << "Error: Invalid value for " << arg << ": " << val << "\n";
            exit_now(1);
        }
    }
    argc = out;
}

Screens check_args(int argc, char* argv[]) {
    if (argc == 1) {
        return Screens::LOADING;
    }

    std::string song_path;
    std::optional<int> difficulty;
    bool auto_play = false;
    bool practice = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--auto") {
            auto_play = true;
        } else if (arg == "--practice") {
            practice = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " <song_path> [difficulty] [--auto] [--practice]\n";
            std::cout << "  song_path   : Path to the TJA song file\n";
            std::cout << "  difficulty  : Difficulty level (optional, defaults to max difficulty)\n";
            std::cout << "  --auto      : Enable auto mode\n";
            std::cout << "  --practice  : Start in practice mode\n";
            std::cout << "Card reader (valid with or without song_path):\n";
            std::cout << "  --card-port <path>   : Serial port (default /dev/ttyUSB0)\n";
            std::cout << "  --card-baud <rate>   : Baud rate (default 38400)\n";
            std::cout << "  --card-poll-ms <ms>  : Poll interval (default 100)\n";
            exit_now(0);
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            exit_now(1);
        } else if (song_path.empty()) {
            song_path = arg;
        } else if (!difficulty.has_value()) {
            try {
                size_t pos = 0;
                int value = std::stoi(arg, &pos);
                if (pos != arg.size()) {
                    throw std::invalid_argument(arg);
                }
                difficulty = value;
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid difficulty value: " << arg << "\n";
                exit_now(1);
            }
        } else {
            std::cerr << "Error: Unexpected extra argument: " << arg << "\n";
            exit_now(1);
        }
    }

    if (song_path.empty()) {
        std::cerr << "Error: song_path is required\n";
        std::cerr << "Use --help for usage information\n";
        exit_now(1);
    }

    std::filesystem::path path = path_from_arg(song_path);
    if (path.empty()) {
        std::cerr << "Error: Song path could not be interpreted: " << song_path << "\n";
        exit_now(1);
    }
    std::error_code path_ec;
    if (!std::filesystem::exists(path, path_ec) || path_ec) {
        std::cerr << "Error: Song file not found: " << song_path << "\n";
        exit_now(1);
    }

    {
        std::error_code abs_ec;
        std::filesystem::path abs = std::filesystem::absolute(path, abs_ec);
        if (!abs_ec) {
            path = abs;
        }
    }
    SongParser tja = [&] -> SongParser {
        try {
            return SongParser(path);
        } catch (const std::exception& e) {
            std::cerr << "Error: Failed to parse song file: " << e.what() << "\n";
            exit_now(1);
        }
    }();

    int selected_difficulty;
    if (difficulty.has_value()) {
        auto& course_data = tja.metadata.course_data;
        if (!course_data.contains(difficulty.value())) {
            std::cerr << "Error: Invalid difficulty: " << difficulty.value() << ". Available: ";
            for (const auto& [key, value] : course_data) {
                std::cerr << key << " ";
            }
            std::cerr << "\n";
            exit_now(1);
        }
        selected_difficulty = difficulty.value();
    } else {
        if (tja.metadata.course_data.empty()) {
            selected_difficulty = static_cast<int>(Difficulty::EASY);
        } else {
            selected_difficulty = tja.metadata.course_data.rbegin()->first;
        }
    }

    Screens current_screen = practice ? Screens::GAME_PRACTICE : Screens::GAME;
    global_data.session_data[(int)PlayerNum::P1].selected_song = std::move(path);
    global_data.session_data[(int)PlayerNum::P1].selected_difficulty = selected_difficulty;
    if (auto_play) {
        global_data.force_auto_play = true;
    }

    return current_screen;
}

struct LoopState {
    std::unordered_map<Screens, std::unique_ptr<Screen>> screens;
    Screens current_screen = Screens::LOADING;
    ray::Camera2D camera = {};
    std::chrono::steady_clock::duration target_duration =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0 / 60.0));
    std::chrono::time_point<std::chrono::steady_clock> next_frame_time = std::chrono::steady_clock::now();
    FPSCounter fps_counter;
    Color last_color = Color::BLACK;
    double screen_fade_start = 0.0;
};

static bool screen_fade_applies(Screens from, Screens to) {
    switch (to) {
    case Screens::GAME:
    case Screens::GAME_2P:
    case Screens::GAME_DAN:
    case Screens::GAME_PRACTICE:
    case Screens::AI_GAME:
    case Screens::RESULT:
    case Screens::RESULT_2P:
    case Screens::DAN_RESULT:
        return false;
    default:
        break;
    }
    if (from == Screens::LOADING) {
        return false;
    }
    if (from == Screens::SONG_SELECT && to == Screens::DAN_SELECT) {
        return false;
    }
    if (from == Screens::ENTRY && to == Screens::DAN_SELECT) {
        return false;
    }
    return true;
}

static LoopState* g_loop = nullptr;
double g_frame_ms = 0.0;

static void populate_screens(std::unordered_map<Screens, std::unique_ptr<Screen>>& screens,
                             std::optional<Screens> except = std::nullopt) {
    if (except != Screens::ENTRY) {
        screens[Screens::ENTRY] = std::make_unique<EntryScreen>();
    }
    if (except != Screens::TITLE) {
        screens[Screens::TITLE] = std::make_unique<TitleScreen>();
    }
    if (except != Screens::SONG_SELECT) {
        screens[Screens::SONG_SELECT] = std::make_unique<SongSelectScreen>();
    }
    if (except != Screens::SONG_SELECT_2P) {
        screens[Screens::SONG_SELECT_2P] = std::make_unique<SongSelect2PScreen>();
    }
    if (except != Screens::LOADING) {
        screens[Screens::LOADING] = std::make_unique<LoadingScreen>();
    }
    if (except != Screens::GAME) {
        screens[Screens::GAME] = std::make_unique<GameScreen>();
    }
    if (except != Screens::GAME_2P) {
        screens[Screens::GAME_2P] = std::make_unique<Game2PScreen>();
    }
    if (except != Screens::GAME_PRACTICE) {
        screens[Screens::GAME_PRACTICE] = std::make_unique<PracticeGameScreen>();
    }
    if (except != Screens::PRACTICE_SELECT) {
        screens[Screens::PRACTICE_SELECT] = std::make_unique<PracticeSongSelectScreen>();
    }
    if (except != Screens::RESULT) {
        screens[Screens::RESULT] = std::make_unique<ResultScreen>();
    }
    if (except != Screens::RESULT_2P) {
        screens[Screens::RESULT_2P] = std::make_unique<Result2PScreen>();
    }
    if (except != Screens::DAN_SELECT) {
        screens[Screens::DAN_SELECT] = std::make_unique<DanSelectScreen>();
    }
    if (except != Screens::GAME_DAN) {
        screens[Screens::GAME_DAN] = std::make_unique<DanGameScreen>();
    }
    if (except != Screens::DAN_RESULT) {
        screens[Screens::DAN_RESULT] = std::make_unique<DanResultScreen>();
    }
    if (except != Screens::SETTINGS) {
        screens[Screens::SETTINGS] = std::make_unique<SettingsScreen>();
    }
    if (except != Screens::INPUT_CALI) {
        screens[Screens::INPUT_CALI] = std::make_unique<InputCaliScreen>();
    }
    if (except != Screens::GAME_OVER) {
        screens[Screens::GAME_OVER] = std::make_unique<GameOverScreen>();
    }
    if (except != Screens::INPUT_TEST) {
        screens[Screens::INPUT_TEST] = std::make_unique<InputTestScreen>();
    }
    if (except != Screens::RETRY_GAME) {
        screens[Screens::RETRY_GAME] = std::make_unique<RetryGameScreen>();
    }
    if (except != Screens::ONBOARDING) {
        screens[Screens::ONBOARDING] = std::make_unique<OnboardingScreen>();
    }
}

void drop_other_screens_for_skin_reload() {
    if (!g_loop) {
        return;
    }
    for (auto it = g_loop->screens.begin(); it != g_loop->screens.end();) {
        if (it->first != g_loop->current_screen) {
            it = g_loop->screens.erase(it);
        } else {
            ++it;
        }
    }
}

void reload_skin_screens() {
    if (!g_loop) {
        return;
    }
    populate_screens(g_loop->screens, g_loop->current_screen);
}

static void run_frame() {
    PROFILE_FRAME();
    PROFILE_SCOPE();
    LoopState& L = *g_loop;

    g_frame_ms = get_current_ms();

    poll_input_events();
#if defined(YATAIDON_PLATFORM_IOS)
    if (ios_is_suspended_state()) {
        return;
    }
#endif
#if defined(YATAIDON_PLATFORM_IOS) || defined(__EMSCRIPTEN__)
    poll_keyboard_once(); // no input thread on these platforms
#endif
    poll_touch_once();

    apply_queued_window_resize();

    if (check_key_pressed(global_data.config->keys.fullscreen_key)) {
        toggle_fullscreen();
        spdlog::info("Toggled fullscreen");
    } else if (check_key_pressed(global_data.config->keys.borderless_key)) {
        toggle_borderless_windowed();
        spdlog::info("Toggled borderless windowed mode");
    }

    { // new sol metatables (see ScriptManager::refresh_method_cache)
        static double last_method_cache_ms = 0.0;
        if (g_frame_ms - last_method_cache_ms > 1000.0) {
            last_method_cache_ms = g_frame_ms;
            script_manager.refresh_method_cache();
        }
    }

    L.camera = compute_camera2d(tex.screen_width, tex.screen_height);
    if (!global_data.skinless) {
        debug_menu.update(L.camera);
    }

    begin_drawing();

    if (global_data.camera.border_color != L.last_color) {
        clear_background(global_data.camera.border_color);
        L.last_color = global_data.camera.border_color;
    }

    begin_mode_2d(L.camera);
    begin_blend_mode(blend_mode::CUSTOM_SEPARATE);

    auto screen_it = L.screens.find(L.current_screen);
    Screen* screen = (screen_it != L.screens.end()) ? screen_it->second.get() : nullptr;
    if (!screen) {
        static Screens last_logged_screen = L.current_screen;
        static double last_log_ms = -1e9;
        if (L.current_screen != last_logged_screen || g_frame_ms - last_log_ms > 1000.0) {
            spdlog::error("Active screen {} is not available, attempting recovery", L.current_screen);
            last_logged_screen = L.current_screen;
            last_log_ms = g_frame_ms;
        }
        populate_screens(L.screens);
        screen_it = L.screens.find(L.current_screen);
        screen = (screen_it != L.screens.end()) ? screen_it->second.get() : nullptr;
    }
    if (!screen) {
        end_blend_mode();
        end_mode_2d();
        end_drawing();
        return;
    }

    network.update(g_frame_ms);
    std::optional<Screens> next_screen;
    {
        PROFILE_SCOPE_N("screen update");
        PROFILE_ZONE_TEXT(screen->screen_name);
        next_screen = screen->update();
    }
    sync_text_input();

    if (!next_screen.has_value() && debug_menu.requested_screen.has_value()) {
        next_screen = screen->on_screen_end(debug_menu.requested_screen.value());
        debug_menu.requested_screen.reset();
    }

    if (screen->screen_init) {
        PROFILE_SCOPE_N("screen draw");
        PROFILE_ZONE_TEXT(screen->screen_name);
        screen->_do_draw();
    }
    if (L.screen_fade_start > 0.0) {
        const SkinInfo* fade_cfg = tex.get_skin_config(SC::SCREEN_FADE_MS);
        const double dur = (fade_cfg && fade_cfg->x > 0) ? fade_cfg->x : 500.0;
        const double t = (g_frame_ms - L.screen_fade_start) / dur;
        if (t >= 1.0) {
            L.screen_fade_start = 0.0;
        } else {
            const float a = static_cast<float>(1.0 - (t < 0.0 ? 0.0 : t));
            draw_rectangle(0, 0, tex.screen_width, tex.screen_height, fade(Color::BLACK, a));
        }
    }

    if (next_screen.has_value()) {
        spdlog::info("Screen changed from {} to {}", L.current_screen, next_screen.value());
        clear_input_buffers();
        if (tex.get_skin_config(SC::SCREEN_FADE_MS) && screen_fade_applies(L.current_screen, next_screen.value())) {
            L.screen_fade_start = g_frame_ms;
        } else {
            L.screen_fade_start = 0.0;
        }
        global_data.previous_screen = global_data.current_screen;
        L.current_screen = next_screen.value();
        global_data.current_screen = screens_to_string(L.current_screen);
        reset_input_lock();
    }

    draw_touch_drum();

    if (!global_data.skinless) {
        if (global_data.config->general.fps_counter) {
            L.fps_counter.update();
            L.fps_counter.draw();
        }

        draw_skin_update_status();

        debug_menu.draw();
    }
    ime_ui_draw(L.camera);

    draw_outer_border(tex.screen_width, tex.screen_height, L.last_color);

    end_blend_mode();
    end_mode_2d();
    {
        PROFILE_SCOPE_N("present");
        end_drawing();

        if (!next_screen.has_value()) {
            swap_screen_buffer();
        }
    }

    if (key_pressed(SDL_SCANCODE_F12)) {
        static int screenshot_counter = 0;
        ray::Image image = load_image_from_screen();
        if (L.current_screen == Screens::RESULT && tex.options[SCO::CROP_RESULT_SCREENSHOT]) {
            image_crop(&image, {0, 0, (float)image.width, (float)image.height / 2.0f});
        }
        export_image(image, text_format("screenshot%03i.png", screenshot_counter));
        unload_image(image);
        screenshot_counter++;
        spdlog::info("Screenshot saved");
    }

#if !defined(__EMSCRIPTEN__) && !defined(YATAIDON_PLATFORM_IOS)
    if (L.target_duration.count() > 0) {
        L.next_frame_time += L.target_duration;
        auto now = std::chrono::steady_clock::now();
        if (L.next_frame_time < now) {
            L.next_frame_time = now;
        }
        auto spin_start = L.next_frame_time - std::chrono::microseconds(500);
        if (spin_start > now) {
#ifdef _WIN32
            // the plain sleep overshoots by up to a 15.6 ms tick, which turned every other frame
            // into a ~17 ms one (frames came as ~1 ms / ~17 ms pairs at any target)
            if (!win32_precise_sleep_us(
                    std::chrono::duration_cast<std::chrono::microseconds>(spin_start - now).count()))
#endif
                std::this_thread::sleep_until(spin_start);
        }
        while (std::chrono::steady_clock::now() < L.next_frame_time) {
        }
    }
#endif
}

int main(int argc, char* argv[]) {
    spdlog::info("Starting YataiDON");
    set_working_directory_to_executable();
    const bool had_config = std::filesystem::exists("config.toml");
    global_data.config = new Config(get_config());
    const bool gen3 = global_data.config->general.score_method == ScoreMethod::GEN3;
    global_data.needs_onboarding = !had_config || !std::filesystem::exists(gen3 ? "scores_gen3.db" : "scores.db");

    save_config(*global_data.config);
    ensure_config_file(*global_data.config);
    parse_card_reader_args(argc, argv, global_data.config->card_reader);
    Screens initial_screen = check_args(argc, argv);
    init_scores_manager(global_data.config->general.score_method == ScoreMethod::GEN3);
    unsigned int flags = window_flag::RESIZABLE;
    if (global_data.config->video.vsync) {
        flags |= window_flag::VSYNC;
        spdlog::info("VSync enabled");
    }
#ifdef YATAIDON_PLATFORM_IOS
    ios_configure_window_flags(flags);
#endif
    set_window_flags(flags);
    quiet_raylib_log();
    setup_logging(global_data.config->general.log_level);

    ime_ui_set_hint();
    init_window(1280, 720, "YataiDON");
    ime_ui_install();
    try {
        load_skin();
    } catch (const std::exception& e) {
        spdlog::error("Starting without a skin: {}", e.what());
        global_data.skinless = true;
        global_data.needs_onboarding = true;
        initial_screen = Screens::ONBOARDING;
    }
    apply_queued_window_resize();

    // Apply config defaults for empty access codes
    if (global_data.config->network.access_code_1.empty()) {
        global_data.config->network.access_code_1 = "0";
    }
    if (global_data.config->network.access_code_2.empty()) {
        global_data.config->network.access_code_2 = "1";
    }

    scores_manager.player_1 = global_data.config->network.access_code_1;
    scores_manager.player_2 = global_data.config->network.access_code_2;
    if (auto pd = scores_manager.get_player_data(scores_manager.player_1)) {
        scores_manager.player_1_data = *pd;
    }
    if (auto pd = scores_manager.get_player_data(scores_manager.player_2)) {
        scores_manager.player_2_data = *pd;
    }

#ifdef PLATFORM_ANDROID
    network.check_and_install_android_update();
#endif
    if (global_data.config->general.skin_updater) {
        skin_updater.start();
    }

#ifdef YATAIDON_PLATFORM_IOS
    ios_initialize_after_window();
#endif

    double target_fps = global_data.config->video.target_fps;
    if (target_fps != -1) {
        spdlog::info("Target FPS set to {}", target_fps);
    }

    g_loop = new LoopState();
    LoopState& L = *g_loop;

    L.current_screen = initial_screen;
    global_data.current_screen = screens_to_string(initial_screen);
    L.target_duration = (target_fps > 0.0) ? std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                                 std::chrono::duration<double>(1.0 / target_fps))
                                           : std::chrono::steady_clock::duration::zero();

    populate_screens(L.screens);

    L.camera = compute_camera2d(tex.screen_width, tex.screen_height);

#if !defined(__EMSCRIPTEN__) && !defined(YATAIDON_PLATFORM_IOS)
    if (global_data.config->video.borderless) {
        toggle_borderless_windowed();
        spdlog::info("Borderless window enabled");
    }
    if (global_data.config->video.fullscreen) {
        toggle_fullscreen();
        spdlog::info("Fullscreen enabled");
    }
#endif

    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD,
                              RL_FUNC_ADD);
    disable_exit_key();
    hide_cursor();

    L.next_frame_time = std::chrono::steady_clock::now();
#ifdef __EMSCRIPTEN__
    return emscripten_run_main_loop(run_frame);
#elif defined(YATAIDON_PLATFORM_IOS)
    return ios_run_main_loop(run_frame);
#else
    input_thread = std::thread(input_polling_thread);
    start_midi_input(global_data.config->midi);

    while (!window_should_close()) {
        process_midi_events();

        // Consume buffered quit-key presses while editing to prevent a delayed game exit after the field closes.
        if (check_key_pressed(global_data.config->keys.exit_key) &&
            !is_text_input_key(global_data.config->keys.exit_key)) {
            break;
        }
        run_frame();
    }

    shutdown_midi_input();
    input_thread_running = false;
    if (input_thread.joinable()) {
        input_thread.join();
    }
    network.shutdown();
    skin_updater.shutdown();
    shutdown_sdl_joysticks();
    delete g_loop;
    g_loop = nullptr;
    // Cleanup needs a live SDL/OpenGL context and config.
    webcam.close();
    unload_skin();
    delete global_data.config;
    global_data.config = nullptr;
#if defined(__linux__) && !defined(__ANDROID__)
    unix_close_window();
#else
    close_window();
#endif
    spdlog::info("Game closed");
#endif
}
