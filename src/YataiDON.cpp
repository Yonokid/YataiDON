#include <optional>
#include <rlgl.h>
#include <string>
#include <filesystem>
#include "libs/ray.h"
#include "libs/global_data.h"
#include "libs/screen.h"
#include "libs/config.h"
#include "scenes/game.h"
#include "libs/utils.h"
#include "libs/audio.h"
#include "libs/logging.h"
#include "objects/fps_counter.h"

namespace fs = std::filesystem;

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

    float scale = std::min(screen_width / virtual_width, screen_height / virtual_height);

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
    audio = std::make_unique<AudioEngine>(
        global_data.config->audio.device_type,
        global_data.config->audio.sample_rate,
        global_data.config->audio.buffer_size,
        global_data.config->volume
    );
    audio->init_audio_device();
    spdlog::info("Audio device initialized");
}

void set_config_flags() {
    if (global_data.config->video.vsync) {
        ray::SetConfigFlags(ray::FLAG_VSYNC_HINT);
        spdlog::info("VSync enabled");
    }
    if (global_data.config->video.target_fps != -1) {
        ray::SetTargetFPS(global_data.config->video.target_fps);
        spdlog::info("Target FPS set to {}", global_data.config->video.target_fps);
    }
    ray::SetConfigFlags(ray::FLAG_MSAA_4X_HINT);
    ray::SetConfigFlags(ray::FLAG_WINDOW_RESIZABLE);
    ray::SetTraceLogLevel(ray::LOG_WARNING);
}

std::string check_args(int argc, char* argv[]) {
    if (argc == 1) {
        return "";
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
    TJAParser tja(path);

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

    //Screens current_screen = practice ? Screens::GAME_PRACTICE : Screens::GAME;
    global_data.session_data[(int)PlayerNum::P1].selected_song = path;
    global_data.session_data[(int)PlayerNum::P1].selected_difficulty = selected_difficulty;
    global_data.modifiers[(int)PlayerNum::P1].auto_play = auto_play;

    return "GAME"; //current_screen
}


int main(int argc, char* argv[]) {
    //force_dedicated_gpu()
    global_data.config = new Config(get_config());
    setup_logging(global_data.config->general.log_level);

    fs::path skin_path = fs::path("Skins") / global_data.config->paths.skin / "Graphics";
    if (fs::exists(skin_path)) {
        tex.init(global_data.config->paths.skin.string());
    } else {
        spdlog::warn("Skin directory not found, skipping texture initialization");
    }

    if (global_data.config->general.score_method == ScoreMethod::GEN3) {
        global_data.score_db = "scores_gen3.db";
    } else {
        global_data.score_db = "scores.db";
    }
    spdlog::info("Starting YataiDON");
    int screen_width = 1280;//global_tex.screen_width
    int screen_height = 720;//global_tex.screen_height

    set_config_flags();
    ray::InitWindow(screen_width, screen_height, "YataiDON");

    spdlog::info("Window initialized: " + std::to_string(screen_width) + "x" + std::to_string(screen_height));
    if (fs::exists(skin_path)) {
        global_tex.init(global_data.config->paths.skin.string());
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

    std::string current_screen = check_args(argc, argv);

    spdlog::info("Initial screen: " + current_screen);

    //create_song_db()

    /*
    //title_screen = TitleScreen('title')
    //entry_screen = EntryScreen('entry')
    //song_select_screen = SongSelectScreen('song_select')
    //song_select_screen_2p = TwoPlayerSongSelectScreen('song_select')
    //load_screen = LoadScreen('loading')
    //game_screen = GameScreen('game')
    //game_screen_2p = TwoPlayerGameScreen('game')
    //game_screen_practice = PracticeGameScreen('game')
    //practice_select_screen = PracticeSongSelectScreen('song_select')
    //ai_select_screen = AISongSelectScreen('song_select')
    //ai_game_screen = AIBattleGameScreen('game')
    //result_screen = ResultScreen('result')
    //result_screen_2p = TwoPlayerResultScreen('result')
    //settings_screen = SettingsScreen('settings')
    //dev_screen = DevScreen('dev')
    //dan_select_screen = DanSelectScreen('dan_select')
    //game_screen_dan = DanGameScreen('game_dan')
    //dan_result_screen = DanResultScreen('dan_result')

    screen_mapping: dict[str, Screen] = {
        Screens.ENTRY: entry_screen,
        Screens.TITLE: title_screen,
        Screens.SONG_SELECT: song_select_screen,
        Screens.SONG_SELECT_2P: song_select_screen_2p,
        Screens.PRACTICE_SELECT: practice_select_screen,
        Screens.GAME: game_screen,
        Screens.GAME_2P: game_screen_2p,
        Screens.GAME_PRACTICE: game_screen_practice,
        Screens.AI_SELECT: ai_select_screen,
        Screens.AI_GAME: ai_game_screen,
        Screens.RESULT: result_screen,
        Screens.RESULT_2P: result_screen_2p,
        Screens.SETTINGS: settings_screen,
        Screens.DEV_MENU: dev_screen,
        Screens.DAN_SELECT: dan_select_screen,
        Screens.GAME_DAN: game_screen_dan,
        Screens.DAN_RESULT: dan_result_screen,
        Screens.LOADING: load_screen
    }
     */

    ray::Camera2D camera = ray::Camera2D();
    update_camera_for_window_size(camera, screen_width, screen_height);
    spdlog::info("Camera2D initialized");

    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    ray::SetExitKey(global_data.config->keys.exit_key);

    ray::HideCursor();
    spdlog::info("Cursor hidden");
    FPSCounter fps_counter = FPSCounter();
    ray::Color last_color = ray::BLACK;

    std::unique_ptr<Screen> screen = std::make_unique<GameScreen>();

    input_thread = std::thread(input_polling_thread);
    spdlog::info("Input polling thread started");


    while (!ray::WindowShouldClose()) {

        if (check_key_pressed(global_data.config->keys.fullscreen_key)) {
            ray::ToggleFullscreen();
            spdlog::info("Toggled fullscreen");
        } else if (check_key_pressed(global_data.config->keys.borderless_key)) {
            ray::ToggleBorderlessWindowed();
            spdlog::info("Toggled borderless windowed mode");
        }

        update_camera_for_window_size(camera, screen_width, screen_height);

        ray::BeginDrawing();
        ray::ClearBackground(ray::BLACK); //remove when finished

        if (global_data.camera.border_color != last_color) {
            ray::ClearBackground(global_data.camera.border_color);
            last_color = global_data.camera.border_color;
        }

        ray::BeginMode2D(camera);
        ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE);

        //screen = screen_mapping[current_screen]

        std::optional<std::string> next_screen = screen->update();

        if (screen->screen_init) {
            screen->_do_draw();
        }

        if (next_screen.has_value()) {
            spdlog::info("Screen changed from {} to {}", current_screen, next_screen.value());
            current_screen = next_screen.value();
            global_data.input_locked = 0;
        }

        if (global_data.config->general.fps_counter) {
            fps_counter.update();
            fps_counter.draw();
        }

        draw_outer_border(screen_width, screen_height, last_color);

        ray::EndBlendMode();
        ray::EndMode2D();
        ray::EndDrawing();
        ray::SwapScreenBuffer();
    }

    input_thread_running = false;
    if (input_thread.joinable()) {
        input_thread.join();
    }
    ray::CloseWindow();
    audio->close_audio_device();
    global_tex.unload_textures();
    //screen_mapping[current_screen].on_screen_end("LOADING")
    spdlog::info("Window closed and audio device shut down");
}
