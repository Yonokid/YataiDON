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
#include "libs/audio_engine.h"

namespace fs = std::filesystem;

inline bool operator==(const ray::Color& a, const ray::Color& b)
{
    return a.r == b.r &&
           a.g == b.g &&
           a.b == b.b &&
           a.a == b.a;
}

inline bool operator!=(const ray::Color& a, const ray::Color& b)
{
    return !(a == b);
}

void update_camera_for_window_size(ray::Camera2D camera, int virtual_width, int virtual_height) {
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

void draw_fps(int last_fps) {
    int curr_fps = ray::GetFPS();
    float pos = 20.0f * global_tex.screen_scale;

    if (curr_fps != 0 && curr_fps != last_fps)
    {
        last_fps = curr_fps;
    }

    ray::Color color;
    ray::Font font = global_data.font;

    if (last_fps < 30) color = ray::RED;
    else if (last_fps < 60) color = ray::YELLOW;
    else color = ray::LIME;

    DrawTextEx(font, ray::TextFormat("%d FPS", last_fps), ray::Vector2{ pos, pos }, pos, 1.0f, color);
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
    audio->setLogLevel(2);
    //old_stderr = os.dup(2)
    //devnull = os.open(os.devnull, os.O_WRONLY)
    //os.dup2(devnull, 2)
    //os.close(devnull)
    audio->initAudioDevice();
    //os.dup2(old_stderr, 2)
    //os.close(old_stderr)
    //logger.info("Audio device initialized")
}

void set_config_flags() {
    if (global_data.config->video.vsync) {
        ray::SetConfigFlags(ray::FLAG_VSYNC_HINT);
        //logger.info("VSync enabled")
    }
    if (global_data.config->video.target_fps != -1) {
        ray::SetTargetFPS(global_data.config->video.target_fps);
        //logger.info(f"Target FPS set to {global_data.config['video']['target_fps']}")
    }
    ray::SetConfigFlags(ray::FLAG_MSAA_4X_HINT);
    ray::SetConfigFlags(ray::FLAG_WINDOW_RESIZABLE);
    ray::SetTraceLogLevel(ray::LOG_WARNING);
}

std::string check_args(int argc, char* argv[]) {
    if (argc == 1) {
        return "";
    }

    // Simple argument parser for C++
    std::string song_path;
    std::optional<int> difficulty;
    bool auto_play = false;
    bool practice = false;

    // Parse arguments
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

    return "GAME";
}

int main(int argc, char* argv[]) {
    //force_dedicated_gpu()

    global_data.config = new Config(get_config());

    // Initialize texture wrapper with skin path from config (if skin exists)
    fs::path skin_path = fs::path("Skins") / global_data.config->paths.skin / "Graphics";
    if (fs::exists(skin_path)) {
        tex.init(global_data.config->paths.skin.string());
    } else {
        ray::TraceLog(ray::LOG_WARNING, "Skin directory not found, skipping texture initialization");
    }

    /*
    switch (global_data.config->general.score_method) {
        case ScoreMethod.GEN3:
            global_data.score_db = 'scores_gen3.db';
        case ScoreMethod.SHINUCHI:
            global_data.score_db = 'scores.db';
    }
     */
    //setup_logging()
    ray::TraceLog(ray::LOG_INFO, "Starting CPPTaiko");
    //TraceLog(LOG_DEBUG, f"Loaded config: {global_data.config}")
    int screen_width = 1280;//global_tex.screen_width
    int screen_height = 720;//global_tex.screen_height

    set_config_flags();
    ray::InitWindow(screen_width, screen_height, "CPPTaiko");

    //logger.info(f"Window initialized: {screen_width}x{screen_height}")
    if (fs::exists(skin_path)) {
        global_tex.init(global_data.config->paths.skin.string());
    } else {
        TraceLog(ray::LOG_WARNING, "Skin directory not found, skipping global texture initialization");
    }
    global_tex.load_screen_textures("global");
    global_tex.load_folder("chara", "chara_0");
    global_tex.load_folder("chara", "chara_1");
    global_tex.load_folder("chara", "chara_4");
    if (global_data.config->video.borderless) {
        ray::ToggleBorderlessWindowed();
        //logger.info("Borderless window enabled")
    }
    if (global_data.config->video.fullscreen) {
        ray::ToggleFullscreen();
        //logger.info("Fullscreen enabled")
    }

    init_audio();

    std::string current_screen = check_args(argc, argv);

    //logger.info(f"Initial screen: {current_screen}")

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
    camera.target = ray::Vector2{0, 0};
    camera.rotation = 0.0;
    update_camera_for_window_size(camera, screen_width, screen_height);
    //logger.info("Camera2D initialized")

    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
    ray::SetExitKey(global_data.config->keys.exit_key);

    ray::HideCursor();
    //logger.info("Cursor hidden")
    int last_fps = 1;
    ray::Color last_color = ray::BLACK;
    int last_discord_check = 0;

    std::unique_ptr<Screen> screen = std::make_unique<GameScreen>();

    while (!ray::WindowShouldClose()) {
        //current_time = get_current_ms()
        /*
        if discord_connected and current_time > last_discord_check + 1000:
            check_discord_heartbeat(current_screen)
            last_discord_check = current_time
        */

        if (ray::IsKeyPressed(global_data.config->keys.fullscreen_key)) {
            ray::ToggleFullscreen();
            //logger.info("Toggled fullscreen")
        } else if (ray::IsKeyPressed(global_data.config->keys.borderless_key)) {
            ray::ToggleBorderlessWindowed();
            //logger.info("Toggled borderless windowed mode")
        }

        update_camera_for_window_size(camera, screen_width, screen_height);

        ray::BeginDrawing();
        ray::ClearBackground(ray::BLACK); //remove when finished

        if (global_data.camera.border_color != last_color) {
            ray::ClearBackground(global_data.camera.border_color);
            last_color = global_data.camera.border_color;
        }

        //BeginMode2D(camera);
        ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE);

        //screen = screen_mapping[current_screen]

        std::optional<std::string> next_screen = screen->update();

        if (screen->screen_init) {
            screen->_do_draw();
        }

        if (next_screen.has_value()) {
            //logger.info(f"Screen changed from {current_screen} to {next_screen}")
            current_screen = next_screen.value();
            global_data.input_locked = 0;
        }

        if (global_data.config->general.fps_counter) {
            draw_fps(last_fps);
        }

        draw_outer_border(screen_width, screen_height, last_color);

        ray::EndBlendMode();
        //EndMode2D();
        ray::EndDrawing();
    }

    ray::CloseWindow();
    audio->closeAudioDevice();
    //if discord_connected:
        //RPC.close()
    global_tex.unload_textures();
    //screen_mapping[current_screen].on_screen_end("LOADING")
    //logger.info("Window closed and audio device shut down")
}
