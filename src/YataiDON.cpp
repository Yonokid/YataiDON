#include <iostream>
#include <rlgl.h>
#ifdef PLATFORM_ANDROID
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#endif

#include "libs/animation.h"
#include "libs/audio.h"
#include "libs/global_data.h"
#include "libs/filesystem.h"
#include "libs/input.h"
#include "libs/logging.h"
#include "libs/camera_utils.h"
#include "libs/network.h"
#include "libs/screen.h"
#include "libs/script.h"
#include "libs/song_parser.h"

#include "scenes/dan_result.h"
#include "scenes/dan_select.h"
#include "scenes/entry.h"
#include "scenes/game.h"
#include "scenes/game_2p.h"
#include "scenes/game_dan.h"
#include "scenes/game_practice.h"
#include "scenes/input_cali.h"
#include "scenes/loading.h"
#include "scenes/result.h"
#include "scenes/result_2p.h"
#include "scenes/sandbox.h"
#include "scenes/settings.h"
#include "scenes/skin_viewer.h"
#include "scenes/song_select.h"
#include "scenes/song_select_2p.h"
#include "scenes/song_select_practice.h"
#include "scenes/title.h"
#include "scenes/game_over.h"

#include "objects/global/fps_counter.h"

#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace fs = std::filesystem;


void draw_outer_border(int screen_width, int screen_height, ray::Color last_color) {
    DrawRectangle(-screen_width, 0, screen_width, screen_height, last_color);
    DrawRectangle(screen_width, 0, screen_width, screen_height, last_color);
    DrawRectangle(0, -screen_height, screen_width, screen_height, last_color);
    DrawRectangle(0, screen_height, screen_width, screen_height, last_color);
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
        } else if (arg == "--sandbox") {
            return Screens::SANDBOX;
        } else if (arg == "--skin-viewer") {
            return Screens::SKIN_VIEWER;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " <song_path> [difficulty] [--auto] [--practice]\n";
            std::cout << "  song_path   : Path to the TJA song file\n";
            std::cout << "  difficulty  : Difficulty level (optional, defaults to max difficulty)\n";
            std::cout << "  --auto      : Enable auto mode\n";
            std::cout << "  --practice  : Start in practice mode\n";
            std::cout << "  --skin-viewer : Open skin viewer\n";
            std::cout << "  --sandbox   : Open sandbox mode\n";
            std::exit(0);
        } else if (song_path.empty()) {
            song_path = arg;
        } else if (!difficulty.has_value()) {
            try {
                difficulty = std::stoi(arg);
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid difficulty value: " << arg << "\n";
                std::exit(1);
            }
        }
    }

    if (song_path.empty()) {
        std::cerr << "Error: song_path is required\n";
        std::cerr << "Use --help for usage information\n";
        std::exit(1);
    }

    std::filesystem::path path(song_path);
    if (!std::filesystem::exists(path)) {
        std::cerr << "Error: Song file not found: " << song_path << "\n";
        std::exit(1);
    }

    path = std::filesystem::absolute(path);
    SongParser tja(path);

    int selected_difficulty;
    if (difficulty.has_value()) {
        auto& course_data = tja.metadata.course_data;
        if (course_data.find(difficulty.value()) == course_data.end()) {
            std::cerr << "Error: Invalid difficulty: " << difficulty.value() << ". Available: ";
            for (const auto& [key, value] : course_data) {
                std::cerr << key << " ";
            }
            std::cerr << "\n";
            std::exit(1);
        }
        selected_difficulty = difficulty.value();
    } else {
        if (tja.metadata.course_data.empty()) {
            selected_difficulty = static_cast<int>(Difficulty::EASY);
        } else {
            selected_difficulty = std::max_element(
                tja.metadata.course_data.begin(),
                tja.metadata.course_data.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; }
            )->first;
        }
    }

    Screens current_screen = practice ? Screens::GAME_PRACTICE : Screens::GAME;
    global_data.session_data[(int)PlayerNum::P1].selected_song = path;
    global_data.session_data[(int)PlayerNum::P1].selected_difficulty = selected_difficulty;
    //global_data.modifiers[(int)PlayerNum::P1].auto_play = auto_play;

    return current_screen;
}

struct LoopState {
    std::unordered_map<Screens, std::unique_ptr<Screen>> screens;
    Screens current_screen = Screens::LOADING;
    ray::Camera2D camera   = {};
    int screen_width       = 0;
    int screen_height      = 0;
    std::chrono::steady_clock::duration target_duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0 / 60.0));
    std::chrono::time_point<std::chrono::steady_clock> next_frame_time = std::chrono::steady_clock::now();
    FPSCounter fps_counter;
    ray::Color last_color = ray::BLACK;
    TextureResizeAnimation* touch_drum_resize = nullptr;
};

static LoopState* g_loop = nullptr;
double g_frame_ms = 0.0;

static void run_frame() {
    LoopState& L = *g_loop;

    g_frame_ms = get_current_ms();

    ray::PollInputEvents();
#ifdef __EMSCRIPTEN__
    poll_keyboard_once();
#endif
    if (global_data.config->general.touch_input)
        poll_touch_once();

    auto frame_start = std::chrono::steady_clock::now();

    if (check_key_pressed(global_data.config->keys.fullscreen_key)) {
        ray::ToggleFullscreen();
        spdlog::info("Toggled fullscreen");
    } else if (check_key_pressed(global_data.config->keys.borderless_key)) {
        ray::ToggleBorderlessWindowed();
        spdlog::info("Toggled borderless windowed mode");
    }

    L.camera = compute_camera2d(L.screen_width, L.screen_height);

    ray::BeginDrawing();

    if (global_data.camera.border_color != L.last_color) {
        ray::ClearBackground(global_data.camera.border_color);
        L.last_color = global_data.camera.border_color;
    }

    ray::BeginMode2D(L.camera);
    ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE);

    Screen* screen = L.screens[L.current_screen].get();

    network.update(get_current_ms());
    std::optional<Screens> next_screen = screen->update();

    if (screen->screen_init) {
        screen->_do_draw();
    }

    if (next_screen.has_value()) {
        spdlog::info("Screen changed from {} to {}", L.current_screen, next_screen.value());
        clear_input_buffers();
        L.current_screen = next_screen.value();
        global_data.input_locked = 0;
    }

    if (global_data.config->general.touch_input) {
        if (touch_drum_pressed.exchange(false, std::memory_order_relaxed))
            L.touch_drum_resize->restart();
        L.touch_drum_resize->update(get_current_ms());
        const float scale = (float)L.touch_drum_resize->attribute;
        float y_fix = 0.0f;
        auto drum_it = global_tex.textures.find(OVERLAY::TOUCH_DRUM);
        if (drum_it != global_tex.textures.end())
            y_fix = drum_it->second->height * 0.5f * (1.0f - scale);
        global_tex.draw_texture(OVERLAY::TOUCH_DRUM, {.scale=scale, .center=true, .y=y_fix, .fade=0.5f});
    }

    if (global_data.config->general.fps_counter) {
        L.fps_counter.update();
        L.fps_counter.draw();
    }

    draw_outer_border(L.screen_width, L.screen_height, L.last_color);

    ray::EndBlendMode();
    ray::EndMode2D();
    ray::EndDrawing();

    if (!next_screen.has_value()) {
        ray::SwapScreenBuffer();
    }

#ifndef __EMSCRIPTEN__
    if (L.target_duration.count() > 0) {
        L.next_frame_time += L.target_duration;
        auto now = std::chrono::steady_clock::now();
        if (L.next_frame_time < now) {
            L.next_frame_time = now;
        }
        auto spin_start = L.next_frame_time - std::chrono::microseconds(500);
        if (spin_start > now) {
            std::this_thread::sleep_until(spin_start);
        }
        while (std::chrono::steady_clock::now() < L.next_frame_time) { }
    }
#endif
}

int main(int argc, char* argv[]) {
    spdlog::info("Starting YataiDON");
    set_working_directory_to_executable();
    init_scores_manager();
    global_data.config = new Config(get_config());
    unsigned int flags = ray::FLAG_WINDOW_RESIZABLE;
    if (global_data.config->video.vsync) {
        flags |= ray::FLAG_VSYNC_HINT;
        spdlog::info("VSync enabled");
    }
    ray::SetConfigFlags(flags);
    ray::SetTraceLogLevel(ray::LOG_ERROR);
    setup_logging(global_data.config->general.log_level);

    fs::path root_skin_path = fs::path("Skins") / global_data.config->paths.skin;

    tex.init(root_skin_path / "Graphics");

#ifdef PLATFORM_ANDROID
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
    ray::InitWindow(tex.screen_width, tex.screen_height, "YataiDON");

    global_tex.init(root_skin_path / "Graphics");
    global_tex.load_screen_textures("global");
    script_manager.init(root_skin_path / "Scripts");
    font_manager.init(root_skin_path / "Graphics/font.ttf");
    audio.init_audio_device(root_skin_path / "Sounds", global_data.config->audio, global_data.config->volume);

    scores_manager.player_1 = global_data.config->general.player_1_id;
    scores_manager.player_2 = global_data.config->general.player_2_id;
    if (auto pd = scores_manager.get_player_data(scores_manager.player_1))
        scores_manager.player_1_data = *pd;
    if (auto pd = scores_manager.get_player_data(scores_manager.player_2))
        scores_manager.player_2_data = *pd;

    if (global_data.config->general.access_code.empty()) {
        std::string access_code = network.register_user(scores_manager.player_1_data.username);
        if (!access_code.empty()) {
            global_data.config->general.access_code = access_code;
            save_config(*global_data.config);
        }
    }

    Screens initial_screen = check_args(argc, argv);

    double target_fps = global_data.config->video.target_fps;
    if (target_fps != -1) {
        spdlog::info("Target FPS set to {}", target_fps);
    }

    g_loop = new LoopState();
    LoopState& L = *g_loop;

    L.screen_width       = tex.screen_width;
    L.screen_height      = tex.screen_height;
    L.current_screen     = initial_screen;
    L.target_duration    = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0 / target_fps));
    L.touch_drum_resize  = (TextureResizeAnimation*)global_tex.get_animation(66);
    L.touch_drum_resize->start();

    L.screens[Screens::ENTRY]           = std::make_unique<EntryScreen>();
    L.screens[Screens::TITLE]           = std::make_unique<TitleScreen>();
    L.screens[Screens::SONG_SELECT]     = std::make_unique<SongSelectScreen>();
    L.screens[Screens::SONG_SELECT_2P]  = std::make_unique<SongSelect2PScreen>();
    L.screens[Screens::LOADING]         = std::make_unique<LoadingScreen>();
    L.screens[Screens::GAME]            = std::make_unique<GameScreen>();
    L.screens[Screens::GAME_2P]         = std::make_unique<Game2PScreen>();
    L.screens[Screens::GAME_PRACTICE]   = std::make_unique<PracticeGameScreen>();
    L.screens[Screens::PRACTICE_SELECT] = std::make_unique<PracticeSongSelectScreen>();
    L.screens[Screens::RESULT]          = std::make_unique<ResultScreen>();
    L.screens[Screens::RESULT_2P]       = std::make_unique<Result2PScreen>();
    L.screens[Screens::DAN_SELECT]      = std::make_unique<DanSelectScreen>();
    L.screens[Screens::GAME_DAN]        = std::make_unique<DanGameScreen>();
    L.screens[Screens::DAN_RESULT]      = std::make_unique<DanResultScreen>();
    L.screens[Screens::SETTINGS]        = std::make_unique<SettingsScreen>();
    L.screens[Screens::INPUT_CALI]      = std::make_unique<InputCaliScreen>();
    L.screens[Screens::SKIN_VIEWER]     = std::make_unique<SkinViewerScreen>();
    L.screens[Screens::SANDBOX]         = std::make_unique<SandboxScreen>();
    L.screens[Screens::GAME_OVER]       = std::make_unique<GameOverScreen>();

    L.camera = compute_camera2d(L.screen_width, L.screen_height);

#ifndef __EMSCRIPTEN__
    if (global_data.config->video.borderless) {
        ray::ToggleBorderlessWindowed();
        spdlog::info("Borderless window enabled");
    }
    if (global_data.config->video.fullscreen) {
        ray::ToggleFullscreen();
        spdlog::info("Fullscreen enabled");
    }
#endif

    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
#if defined(PLATFORM_ANDROID) || defined(__EMSCRIPTEN__)
    ray::SetExitKey(ray::KEY_NULL);
#else
    ray::SetExitKey(global_data.config->keys.exit_key);
#endif
    ray::HideCursor();

    L.next_frame_time = std::chrono::steady_clock::now();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(run_frame, 0, 1);
#else
    input_thread = std::thread(input_polling_thread);

    while (!ray::WindowShouldClose()) {
        run_frame();
    }

    input_thread_running = false;
    if (input_thread.joinable()) {
        input_thread.join();
    }
    shutdown_sdl_joysticks();
    delete g_loop;
    global_tex.unload_textures();
    tex.unload_textures();
    script_manager.shutdown();
    ray::CloseWindow();
    audio.close_audio_device();
    spdlog::info("Game closed");
#endif
}
