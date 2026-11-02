#include "platform_ps5.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <stdexcept>

#include "../../platform/ps5/native-ime.h"
#include "../libs/file/config.h"
#include "../libs/gfx.h"
#include "../libs/gfx/sdl_compat.h"
#include "../libs/global_data.h"
#include "platform.h"
#include "ps5_video.h"

void platform_poll_input() {}

bool platform_resolve_file_path(const std::filesystem::path&, std::filesystem::path&) {
    return false;
}

void platform_initialize_animation_worker() {}

bool platform_present_paces_frames() {
    return false;
}
bool platform_threaded_renderer() {
    return false;
}
bool platform_list_directory(const std::filesystem::path&, std::vector<PlatformFileEntry>&) {
    return false;
}
void platform_begin_file_scan() {}

extern "C" int sceSystemServiceHideSplashScreen();
extern "C" int sceKernelDebugOutText(int channel, const char* text);
extern "C" void av_log_set_level(int level);

void ps5_report_boot_error(const char* message) {
    char text[2048];
    std::snprintf(text, sizeof(text), "[yataidon-ps5] boot error: %s\n", message);
    std::fputs(text, stderr);
    std::fflush(stderr);
    sceKernelDebugOutText(0, text);
}

std::filesystem::path platform_get_working_directory() {
    // Assets, configuration and databases live together in writable storage.
    const std::filesystem::path root = "/data/YataiDON";
    std::filesystem::create_directories(root);
    return root;
}

void platform_install_crash_handlers() {
    // logging.cpp installs the common C++ terminate handler.
}

extern "C" int yataidon_ps5_display_size(int* width, int* height);
extern "C" int ps5_set_output_refresh(int hz) __attribute__((weak));
static int output_refresh = 60;

void platform_init_window() {
    ps5_init_window(global_data.config->video.target_fps);
}

void ps5_init_window(int refresh_hz) {
    // Set this before display discovery creates the first Vulkan instance.
    // RADV otherwise defaults to the installed application's /app0 directory.
    constexpr const char* shader_cache = "/data/YataiDON/radv-shader-cache";
    std::filesystem::create_directories(shader_cache);
    if (setenv("MESA_SHADER_CACHE_DIR", shader_cache, 1) != 0) {
        throw std::runtime_error("PS5 RADV shader cache configuration failed");
    }
    std::fprintf(stderr, "[yataidon-ps5] RADV shader cache: %s\n", shader_cache);

    ps5_preload_video_decoder();
    av_log_set_level(16);
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) < 0) {
        throw std::runtime_error(SDL_GetError());
    }
    int width = 0, height = 0;
    if (yataidon_ps5_display_size(&width, &height) != 0 || width <= 0 || height <= 0) {
        throw std::runtime_error("PS5 Vulkan display discovery failed");
    }
    init_window(width, height, "YataiDON");
    begin_drawing();
    clear_background(Color::BLACK);
    end_drawing();
    swap_screen_buffer();
    sceSystemServiceHideSplashScreen();
    const int accepted = ps5_apply_output_refresh(refresh_hz);
    if (accepted < 0) {
        std::fprintf(stderr, "[yataidon-ps5] Vulkan refresh request failed; retaining 60 Hz\n");
    }
}

int ps5_apply_output_refresh(int refresh_hz) {
    // The native bridge owns VideoOut and validates HDMI 120 Hz capability.
    const int accepted = ps5_set_output_refresh ? ps5_set_output_refresh(refresh_hz) : -1;
    if (accepted > 0) {
        output_refresh = accepted;
    }
    return accepted;
}

void platform_close_window() {
    close_window();
}

bool platform_retains_assets() {
    return true;
}

int platform_run_main(int (*game_main)(int, char**), int argc, char** argv) {
    try {
        return game_main(argc, argv);
    } catch (const std::exception& error) {
        ps5_report_boot_error(error.what());
    } catch (...) {
        ps5_report_boot_error("unknown C++ exception");
    }
    return 1;
}

unsigned platform_window_flags(unsigned flags) {
    return flags; // fixed-size display
}

int platform_apply_target_fps(int fps) {
    const int requested = fps == 60 ? 60 : 120;
    if (requested == output_refresh) {
        return output_refresh;
    }
    const int active = ps5_apply_output_refresh(requested);
    // Fall back to whatever the display kept rather than retrying a rejected mode.
    return active == 60 || active == 120 ? active : output_refresh;
}

// DualSense drum layout (raylib gamepad button numbers).
namespace ps5_controls {
    inline constexpr std::array left_ka{1, 4, 9, 10};
    inline constexpr std::array left_don{2, 3, 16};
    inline constexpr std::array right_ka{5, 6, 11, 12};
    inline constexpr std::array right_don{7, 8, 17};
    template<class Layout>
    bool apply_default_layout_if_empty(Layout& layout) {
        if (!layout.left_kat.empty() || !layout.left_don.empty() || !layout.right_kat.empty() ||
            !layout.right_don.empty()) {
            return false;
        }
        layout.left_kat.assign(left_ka.begin(), left_ka.end());
        layout.left_don.assign(left_don.begin(), left_don.end());
        layout.right_kat.assign(right_ka.begin(), right_ka.end());
        layout.right_don.assign(right_don.begin(), right_don.end());
        return true;
    }

} // namespace ps5_controls

void platform_load_config(Config& config, const toml::table& file) {
    // Apply the requested DualSense layout once; retain later rebindings.
    if (file["ps5"]["controller_layout_version"].value_or(0) < 2) {
        config.gamepad_1p.left_kat.assign(ps5_controls::left_ka.begin(), ps5_controls::left_ka.end());
        config.gamepad_1p.left_don.assign(ps5_controls::left_don.begin(), ps5_controls::left_don.end());
        config.gamepad_1p.right_kat.assign(ps5_controls::right_ka.begin(), ps5_controls::right_ka.end());
        config.gamepad_1p.right_don.assign(ps5_controls::right_don.begin(), ps5_controls::right_don.end());
        spdlog::info("Applied requested DualSense drum layout v2");
    }
    // Upgrade the previously empty P2 defaults once, preserving custom layouts.
    if (file["ps5"]["p2_controller_layout_version"].value_or(0) < 1 &&
        ps5_controls::apply_default_layout_if_empty(config.gamepad_2p)) {
        spdlog::info("Applied DualSense P2 drum layout v1");
    }
    config.keys.gamepad_menu_layout = file["keys"]["gamepad_menu_layout"].value_or(true);
    config.video.vsync = true;
    config.video.fullscreen = config.video.borderless = false;
    config.video.target_fps = config.video.target_fps == 60 ? 60 : 120;
}

void platform_save_config(const Config&, toml::table& out) {
    out.insert("ps5", toml::table{{"controller_layout_version", 2}, {"p2_controller_layout_version", 1}});
}

bool platform_offers_setting(std::string_view path, std::string_view value) {
    if (path == "video/vsync" || path == "video/fullscreen" || path == "video/borderless" ||
        path == "keys/fullscreen_key" || path == "keys/borderless_key" || path == "audio/device" ||
        path == "audio/device_type" || path == "audio/sample_rate") {
        return false;
    }
    // The display runs at 60 or 120 Hz (120 needs a compatible display).
    return path != "video/target_fps" || value.empty() || value == "60" || value == "120";
}

bool platform_read_file(const char* path, std::vector<unsigned char>& out) {
    int length = 0;
    unsigned char* bytes = ps5_load_file_data(path, &length);
    out.assign(bytes, bytes + (bytes ? length : 0));
    std::free(bytes);
    return true;
}

PlatformAudioDevice* platform_open_audio(PlatformMixCallback mix, void* context, double& sample_rate,
                                         unsigned long& frames) {
    // AudioOut runs at 48 kHz in multiples of 256 frames.
    const unsigned long port_frames = std::clamp<unsigned long>(((frames + 255) / 256) * 256, 256, 2048);
    PlatformAudioDevice* device = ps5_open_audio(mix, context, static_cast<unsigned int>(port_frames));
    if (device) {
        sample_rate = 48000;
        frames = port_frames;
        spdlog::info("PS5 AudioOut: 48000 Hz, stereo, {} frames", frames);
    }
    return device;
}

void platform_close_audio(PlatformAudioDevice* device) {
    ps5_close_audio(device);
}

std::unique_ptr<PlatformVideoDecoder> platform_open_video_decoder(const std::filesystem::path& path,
                                                                  const std::atomic<bool>& stop) {
    return PS5HardwareVideo::open(path, stop);
}

bool platform_take_text_replacement() {
    return yataidon_ime_take_replacement() != 0;
}

bool platform_async_text_build() {
    return true;
}

bool platform_file_stamp(const std::filesystem::path&, int64_t&, int64_t&) {
    return false;
}

// 100% of the render output's pixel density: the cropped target saves unused space
// without reducing resolution.
float platform_supersample() {
    return 1.0f;
}

bool platform_preload_song_previews() {
    return true;
}
