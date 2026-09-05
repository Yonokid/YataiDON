#include "global_data.h"
#include "filesystem.h"
#include "texture.h"
#include "script.h"
#include "text.h"
#include "audio.h"

GlobalData global_data;

void load_skin() {
    ensure_skin_extracted(global_data.config->paths.skin.string());
    fs::path root_skin_path = fs::path("Skins") / global_data.config->paths.skin;
    set_skin_graphics_path(root_skin_path / "Graphics");

    tex.init(root_skin_path / "Graphics");
    const bool was_fullscreen = ray::IsWindowFullscreen();
    if (was_fullscreen) ray::ToggleFullscreen();
    ray::SetWindowSize(tex.screen_width, tex.screen_height);
    if (was_fullscreen) ray::ToggleFullscreen();

    global_tex.init(root_skin_path / "Graphics");
    global_tex.load_screen_textures("global");
    script_manager.init(root_skin_path / "Scripts");
    fs::path font_path = resolve_skin_path("Graphics/font.ttf");
    font_manager.init(font_path);
    audio.init_audio_device(root_skin_path / "Sounds", global_data.config->audio, global_data.config->volume);
}

void unload_skin() {
    tex.unload_textures();
    global_tex.unload_textures();
    script_manager.shutdown();
    font_manager.unload();
    audio.unload_all_sounds();
    audio.unload_all_music();
    audio.close_audio_device();
}

void reset_session() {
    global_data.session_data[1] = SessionData();
    global_data.session_data[2] = SessionData();
}

int get_player_id(PlayerNum player_num) {
    return (player_num == global_data.first_login_player)
        ? global_data.config->general.player_1_id
        : global_data.config->general.player_2_id;
}
