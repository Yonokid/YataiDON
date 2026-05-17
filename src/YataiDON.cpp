#include <cmath>
#include <iostream>
#include <rlgl.h>

#include "libs/audio.h"
#include "libs/global_data.h"
#include "libs/filesystem.h"
#include "libs/input.h"
#include "libs/logging.h"
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

namespace fs = std::filesystem;

void update_camera_for_window_size(ray::Camera3D& camera, int virtual_width, int virtual_height) {
    int screen_width = ray::GetScreenWidth();
    int screen_height = ray::GetScreenHeight();

    float rotation_rad = global_data.camera.rotation * DEG2RAD;
    camera.up         = { std::sin(rotation_rad), -std::cos(rotation_rad), 0.0f };
    camera.projection = ray::CAMERA_ORTHOGRAPHIC;

    if (screen_width == 0 || screen_height == 0) {
        camera.fovy     = (float)virtual_height;
        camera.position = { virtual_width * 0.5f, virtual_height * 0.5f, -100.0f };
        camera.target   = { virtual_width * 0.5f, virtual_height * 0.5f, 0.0f };
        return;
    }

    float scale = std::min((float)screen_width / virtual_width, (float)screen_height / virtual_height);

    float base_offset_x = (screen_width - (virtual_width * scale)) * 0.5f;
    float base_offset_y = (screen_height - (virtual_height * scale)) * 0.5f;

    float effective_zoom = scale * global_data.camera.zoom;

    float zoom_offset_x = (virtual_width * scale * (global_data.camera.zoom - 1.0f)) * 0.5f;
    float zoom_offset_y = (virtual_height * scale * (global_data.camera.zoom - 1.0f)) * 0.5f;

    float h_scale = global_data.camera.h_scale;
    float v_scale = global_data.camera.v_scale;

    float h_scale_offset_x = (virtual_width * scale * (h_scale - 1.0f)) * 0.5f;
    float v_scale_offset_y = (virtual_height * scale * (v_scale - 1.0f)) * 0.5f;

    float offset_x = base_offset_x - zoom_offset_x - h_scale_offset_x + (global_data.camera.offset.x * scale);
    float offset_y = base_offset_y - zoom_offset_y - v_scale_offset_y + (global_data.camera.offset.y * scale);

    float world_center_x = (screen_width * 0.5f - offset_x) / effective_zoom;
    float world_center_y = (screen_height * 0.5f - offset_y) / effective_zoom;

    camera.fovy     = (float)screen_height / effective_zoom;
    camera.position = { world_center_x, world_center_y, -100.0f };
    camera.target   = { world_center_x, world_center_y, 0.0f };
}

void draw_outer_border(int screen_width, int screen_height, ray::Color last_color) {
    DrawRectangle(-screen_width, 0, screen_width, screen_height, last_color);
    DrawRectangle(screen_width, 0, screen_width, screen_height, last_color);
    DrawRectangle(0, -screen_height, screen_width, screen_height, last_color);
    DrawRectangle(0, screen_height, screen_width, screen_height, last_color);
}

Screens check_args(int argc, char* argv[]) {
    if (argc == 1) {
        return Screens::ENTRY;
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
    global_data.modifiers[(int)PlayerNum::P1].auto_play = auto_play;

    return current_screen;
}

struct LoopState {
    std::unordered_map<Screens, std::unique_ptr<Screen>> screens;
    Screens current_screen = Screens::LOADING;
    ray::Camera3D camera   = {};
    int screen_width       = 0;
    int screen_height      = 0;
    std::chrono::duration<double> target_duration = std::chrono::duration<double>(1.0 / 60.0);
    FPSCounter fps_counter;
    ray::Color last_color = ray::BLACK;
};

static LoopState* g_loop = nullptr;

static void run_frame() {
    LoopState& L = *g_loop;

    ray::PollInputEvents();

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

    ray::BeginMode3D(L.camera);
    rlDisableDepthTest();
    ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE);

    Screen* screen = L.screens[L.current_screen].get();

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
    ray::EndMode3D();
    ray::EndDrawing();
    ray::SwapScreenBuffer();

    auto elapsed   = std::chrono::steady_clock::now() - frame_start;
    auto remaining = L.target_duration - elapsed;
    if (remaining > std::chrono::duration<double>::zero()) {
        std::this_thread::sleep_for(remaining);
    }
}

int main(int argc, char* argv[]) {
    spdlog::info("Starting YataiDON");
    set_working_directory_to_executable();
    global_data.config = new Config(get_config());
    if (global_data.config->video.vsync) {
        ray::SetConfigFlags(ray::FLAG_VSYNC_HINT);
        spdlog::info("VSync enabled");
    }
    ray::SetConfigFlags(ray::FLAG_MSAA_4X_HINT);
    ray::SetConfigFlags(ray::FLAG_WINDOW_RESIZABLE);
    ray::SetTraceLogLevel(ray::LOG_ERROR);
    setup_logging(global_data.config->general.log_level);

    fs::path root_skin_path = fs::path("Skins") / global_data.config->paths.skin;

    tex.init(root_skin_path / "Graphics");

    ray::InitWindow(tex.screen_width, tex.screen_height, "YataiDON");

    global_tex.init(root_skin_path / "Graphics");
    global_tex.load_screen_textures("global");
    script_manager.init(root_skin_path / "Scripts");
    font_manager.init(root_skin_path / "Graphics/font.ttf");
    audio.init_audio_device(root_skin_path / "Sounds", global_data.config->audio, global_data.config->volume);

    scores_manager.player_1 = scores_manager.add_player(global_data.config->nameplate_1p.name);
    scores_manager.player_2 = scores_manager.add_player(global_data.config->nameplate_2p.name);

    Screens initial_screen = check_args(argc, argv);

    double target_fps = global_data.config->video.target_fps;
    if (target_fps != -1) {
        spdlog::info("Target FPS set to {}", target_fps);
    }

    g_loop = new LoopState();
    LoopState& L = *g_loop;

    L.screen_width    = tex.screen_width;
    L.screen_height   = tex.screen_height;
    L.current_screen  = initial_screen;
    L.target_duration = std::chrono::duration<double>(1.0 / target_fps);

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

    update_camera_for_window_size(L.camera, L.screen_width, L.screen_height);

    if (global_data.config->video.borderless) {
        ray::ToggleBorderlessWindowed();
        spdlog::info("Borderless window enabled");
    }
    if (global_data.config->video.fullscreen) {
        ray::ToggleFullscreen();
        spdlog::info("Fullscreen enabled");
    }

    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    ray::SetExitKey(global_data.config->keys.exit_key);
    ray::HideCursor();

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
}
