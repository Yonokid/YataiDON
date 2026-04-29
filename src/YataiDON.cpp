#include <optional>
#include <rlgl.h>
#include <string>
#include <filesystem>
#include <clocale>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "libs/ray.h"
#include "libs/global_data.h"
#include "libs/scores.h"
#include "libs/screen.h"
#include "libs/config.h"
#include "libs/script.h"
#include "libs/input.h"
#include "libs/audio.h"
#ifdef AUDIO_BACKEND_RAYLIB
#include "libs/audio_raylib.h"
#endif
#include "libs/logging.h"
#include "objects/global/fps_counter.h"

#include "raylib.h"
#include "scenes/game.h"
#include "scenes/game_practice.h"
#include "scenes/game_2p.h"
#include "scenes/result.h"
#include "scenes/result_2p.h"
#include "scenes/title.h"
#include "scenes/loading.h"
#include "scenes/entry.h"
#include "scenes/song_select.h"
#include "scenes/song_select_practice.h"
#include "scenes/song_select_2p.h"
#include "scenes/settings.h"
#include "scenes/input_cali.h"
#include "scenes/skin_viewer.h"

#ifdef _WIN32
    #include <windows.h>
#endif

namespace fs = std::filesystem;

void set_working_directory_to_executable() {
#ifdef __EMSCRIPTEN__
    // Preloaded files are mounted at the root of the virtual filesystem.
    std::filesystem::current_path("/");
    return;
#elif defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::filesystem::path exe_path(buffer);
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len == -1) {
        spdlog::error("Failed to get executable path");
        return;
    }
    buffer[len] = '\0';
    std::filesystem::path exe_path(buffer);
#endif

#ifndef __EMSCRIPTEN__
    std::filesystem::path exe_dir = exe_path.parent_path();
    std::filesystem::current_path(exe_dir);
    spdlog::info("Working directory set to: {}", exe_dir.string());
#endif
}

void update_camera_for_window_size(ray::Camera2D& camera, int virtual_width, int virtual_height) {
    int screen_width = ray::GetScreenWidth();
    int screen_height = ray::GetScreenHeight();

    if (screen_width == 0 || screen_height == 0) {
        camera.zoom = 1.0;
        camera.offset.x = 0;
        camera.offset.y = 0;
        camera.rotation = 0.0;
        return;
    }

    float scale = std::min((float)screen_width / virtual_width, (float)screen_height / virtual_height);

    float base_offset_x = (screen_width - (virtual_width * scale)) * 0.5;
    float base_offset_y = (screen_height - (virtual_height * scale)) * 0.5;

    camera.zoom = scale * global_data.camera.zoom;

    float zoom_offset_x = (virtual_width * scale * (global_data.camera.zoom - 1.0)) * 0.5;
    float zoom_offset_y = (virtual_height * scale * (global_data.camera.zoom - 1.0)) * 0.5;

    float h_scale = global_data.camera.h_scale;
    float v_scale = global_data.camera.v_scale;

    float h_scale_offset_x = (virtual_width * scale * (h_scale - 1.0)) * 0.5;
    float v_scale_offset_y = (virtual_height * scale * (v_scale - 1.0)) * 0.5;

    camera.offset.x = base_offset_x - zoom_offset_x - h_scale_offset_x + (global_data.camera.offset.x * scale);
    camera.offset.y = base_offset_y - zoom_offset_y - v_scale_offset_y + (global_data.camera.offset.y * scale);

    camera.rotation = global_data.camera.rotation;
}

void draw_outer_border(int screen_width, int screen_height, ray::Color last_color) {
    DrawRectangle(-screen_width, 0, screen_width, screen_height, last_color);
    DrawRectangle(screen_width, 0, screen_width, screen_height, last_color);
    DrawRectangle(0, -screen_height, screen_width, screen_height, last_color);
    DrawRectangle(0, screen_height, screen_width, screen_height, last_color);
}

void init_audio() {
#ifdef AUDIO_BACKEND_RAYLIB
    audio = std::make_unique<RaylibAudioEngine>(global_data.config->volume);
#else
    audio = std::make_unique<AudioEngine>(
        global_data.config->audio.device_type,
        global_data.config->audio.sample_rate,
        global_data.config->audio.buffer_size,
        global_data.config->volume
    );
#endif
    fs::path skin_sounds = fs::path("Skins") / global_data.config->paths.skin / "Sounds";
    if (fs::exists(skin_sounds)) {
        audio->sounds_path = skin_sounds;
    } else {
        spdlog::error("Skin sounds directory not found, audio may not load correctly");
    }
    audio->init_audio_device();
    spdlog::info("Audio device initialized");
}

void set_config_flags() {
    if (global_data.config->video.vsync) {
        ray::SetConfigFlags(ray::FLAG_VSYNC_HINT);
        spdlog::info("VSync enabled");
    }
    ray::SetConfigFlags(ray::FLAG_MSAA_4X_HINT);
    ray::SetConfigFlags(ray::FLAG_WINDOW_RESIZABLE);
    ray::SetTraceLogLevel(ray::LOG_ERROR);
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
    global_data.modifiers[(int)PlayerNum::P1].auto_play = auto_play;

    return current_screen;
}

struct LoopState {
    std::unordered_map<Screens, Screen*> screen_mapping;
    Screens current_screen = Screens::LOADING;
    ray::Camera2D camera   = {};
    int screen_width       = 0;
    int screen_height      = 0;
    std::chrono::duration<double> target_duration = std::chrono::duration<double>(1.0 / 60.0);
    FPSCounter fps_counter;
    ray::Color last_color = ray::BLACK;

    std::unique_ptr<TitleScreen>     title_screen;
    std::unique_ptr<EntryScreen>     entry_screen;
    std::unique_ptr<SongSelectScreen> song_select_screen;
    std::unique_ptr<SongSelect2PScreen> song_select_2p_screen;
    std::unique_ptr<LoadingScreen>   load_screen;
    std::unique_ptr<GameScreen>              game_screen;
    std::unique_ptr<Game2PScreen>            game_2p_screen;
    std::unique_ptr<PracticeGameScreen>      practice_game_screen;
    std::unique_ptr<PracticeSongSelectScreen> practice_select_screen;
    std::unique_ptr<ResultScreen>            result_screen;
    std::unique_ptr<Result2PScreen>          result_2p_screen;
    std::unique_ptr<SettingsScreen>          settings_screen;
    std::unique_ptr<InputCaliScreen>         input_cali_screen;
    std::unique_ptr<SkinViewerScreen>        skin_viewer_screen;
};

static LoopState* g_loop = nullptr;

static void run_frame() {
    LoopState& L = *g_loop;

#ifdef __EMSCRIPTEN__
    poll_keyboard_once();
#endif
    ray::PollInputEvents();
    audio->update();  // no-op for PortAudio; required by raylib raudio backend

    auto frame_start = std::chrono::steady_clock::now();

    if (check_key_pressed(global_data.config->keys.fullscreen_key)) {
        ray::ToggleFullscreen();
        spdlog::info("Toggled fullscreen");
    } else if (check_key_pressed(global_data.config->keys.borderless_key)) {
        ray::ToggleBorderlessWindowed();
        spdlog::info("Toggled borderless windowed mode");
    }

    update_camera_for_window_size(L.camera, L.screen_width, L.screen_height);

    ray::BeginDrawing();

    if (global_data.camera.border_color != L.last_color) {
        ray::ClearBackground(global_data.camera.border_color);
        L.last_color = global_data.camera.border_color;
    }

    ray::BeginMode2D(L.camera);
    ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE);

    Screen* screen = L.screen_mapping[L.current_screen];

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

    if (global_data.config->general.fps_counter) {
        L.fps_counter.update();
        L.fps_counter.draw();
    }

    draw_outer_border(L.screen_width, L.screen_height, L.last_color);

    ray::EndBlendMode();
    ray::EndMode2D();
    ray::EndDrawing();
    ray::SwapScreenBuffer();

#ifndef __EMSCRIPTEN__
    auto elapsed   = std::chrono::steady_clock::now() - frame_start;
    auto remaining = L.target_duration - elapsed;
    if (remaining > std::chrono::duration<double>::zero()) {
        std::this_thread::sleep_for(remaining);
    }
#endif
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, ".UTF-8");
#else
    setlocale(LC_ALL, "");
#endif
    set_working_directory_to_executable();
    global_data.config = new Config(get_config());
    setup_logging(global_data.config->general.log_level);

    fs::path skin_path = fs::path("Skins") / global_data.config->paths.skin / "Graphics";
    if (fs::exists(skin_path)) {
        tex.init(skin_path);
    } else {
        spdlog::warn("Skin directory not found, skipping texture initialization");
    }

    fs::path script_path = fs::path("Skins") / global_data.config->paths.skin / "Scripts";
    if (fs::exists(script_path)) {
        script_manager.init(script_path);
    } else {
        spdlog::warn("Skin directory not found, skipping script initialization");
    }

    scores_manager.player_1 = scores_manager.add_player(global_data.config->nameplate_1p.name);
    scores_manager.player_2 = scores_manager.add_player(global_data.config->nameplate_2p.name);
    spdlog::info("Starting YataiDON");
    spdlog::info("Screen size: " + std::to_string(tex.screen_width) + "x" + std::to_string(tex.screen_height));

    set_config_flags();
    ray::InitWindow(tex.screen_width, tex.screen_height, "YataiDON");

    fs::path font_path = fs::path("Skins") / global_data.config->paths.skin / "Graphics/font.ttf";
    if (fs::exists(font_path)) {
        font_manager.init(font_path);
    } else {
        spdlog::warn("Font file not found, skipping font initialization");
    }

    spdlog::info("Window initialized: " + std::to_string(tex.screen_width) + "x" + std::to_string(tex.screen_height));
    if (fs::exists(skin_path)) {
        global_tex.init(skin_path);
    } else {
        spdlog::warn("Skin directory not found, skipping global texture initialization");
    }
    global_tex.load_screen_textures("global");
    //global_tex.load_folder("chara", "chara_0");
    //global_tex.load_folder("chara", "chara_1");
    //global_tex.load_folder("chara", "chara_4");
    if (global_data.config->video.borderless) {
        ray::ToggleBorderlessWindowed();
        spdlog::info("Borderless window enabled");
    }
    if (global_data.config->video.fullscreen) {
        ray::ToggleFullscreen();
        spdlog::info("Fullscreen enabled");
    }

    init_audio();

    Screens initial_screen = check_args(argc, argv);

    double target_fps = global_data.config->video.target_fps;
    if (target_fps != -1) {
        spdlog::info("Target FPS set to {}", target_fps);
    }

    // Build loop state
    g_loop = new LoopState();
    LoopState& L = *g_loop;

    L.screen_width    = tex.screen_width;
    L.screen_height   = tex.screen_height;
    L.current_screen  = initial_screen;
    L.target_duration = std::chrono::duration<double>(1.0 / target_fps);

    L.title_screen           = std::make_unique<TitleScreen>();
    L.entry_screen           = std::make_unique<EntryScreen>();
    L.song_select_screen     = std::make_unique<SongSelectScreen>();
    L.song_select_2p_screen  = std::make_unique<SongSelect2PScreen>();
    L.load_screen            = std::make_unique<LoadingScreen>();
    L.game_screen            = std::make_unique<GameScreen>();
    L.game_2p_screen         = std::make_unique<Game2PScreen>();
    L.practice_game_screen   = std::make_unique<PracticeGameScreen>();
    L.practice_select_screen = std::make_unique<PracticeSongSelectScreen>();
    L.result_screen          = std::make_unique<ResultScreen>();
    L.result_2p_screen       = std::make_unique<Result2PScreen>();
    L.settings_screen        = std::make_unique<SettingsScreen>();
    L.input_cali_screen      = std::make_unique<InputCaliScreen>();
    L.skin_viewer_screen     = std::make_unique<SkinViewerScreen>();

    L.screen_mapping = {
        {Screens::ENTRY,            L.entry_screen.get()},
        {Screens::TITLE,            L.title_screen.get()},
        {Screens::SONG_SELECT,      L.song_select_screen.get()},
        {Screens::SONG_SELECT_2P,   L.song_select_2p_screen.get()},
        {Screens::GAME,             L.game_screen.get()},
        {Screens::GAME_2P,          L.game_2p_screen.get()},
        {Screens::GAME_PRACTICE,    L.practice_game_screen.get()},
        {Screens::PRACTICE_SELECT,  L.practice_select_screen.get()},
        {Screens::RESULT,           L.result_screen.get()},
        {Screens::RESULT_2P,        L.result_2p_screen.get()},
        {Screens::SETTINGS,         L.settings_screen.get()},
        {Screens::INPUT_CALI,       L.input_cali_screen.get()},
        {Screens::LOADING,          L.load_screen.get()},
        {Screens::SKIN_VIEWER,      L.skin_viewer_screen.get()},
    };

    update_camera_for_window_size(L.camera, L.screen_width, L.screen_height);
    spdlog::info("Camera2D initialized");

    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    ray::SetExitKey(global_data.config->keys.exit_key);
    ray::HideCursor();
    spdlog::info("Cursor hidden");

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(run_frame, 0, 1);
#else
    input_thread = std::thread(input_polling_thread);
    spdlog::info("Input polling thread started");

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
    script_manager.tex.unload_textures();
    script_manager.shutdown();
    ray::CloseWindow();
    audio->close_audio_device();
    spdlog::info("Window closed and audio device shut down");
#endif
}
