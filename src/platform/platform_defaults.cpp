// platform.h hooks shared by every build, with per-OS branches where the allocator differs.
#include "../libs/file/config.h"
#include "../libs/gfx.h"
#include "../libs/input/ime_ui.h"
#include "platform.h"
#if defined(__GLIBC__) || defined(__ANDROID__)
#include <malloc.h>
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#endif

void platform_configure_allocator() {
#if defined(__GLIBC__)
    mallopt(M_ARENA_MAX, 2);
    mallopt(M_MMAP_THRESHOLD, 128 * 1024);
    mallopt(M_TRIM_THRESHOLD, 128 * 1024);
#elif defined(__ANDROID__)
#if defined(M_DECAY_TIME)
    mallopt(M_DECAY_TIME, 0); // return unused pages immediately
#endif
#endif
}

void platform_trim_heap() {
#if defined(__GLIBC__)
    malloc_trim(0);
#elif defined(__ANDROID__)
#if defined(M_PURGE_ALL)
    mallopt(M_PURGE_ALL, 0);
#endif
#if defined(M_PURGE)
    mallopt(M_PURGE, 0);
#endif
#elif defined(__APPLE__)
    malloc_zone_pressure_relief(nullptr, 0);
#endif
}

void platform_poll_input() {}

void platform_initialize_animation_worker() {}

bool platform_present_paces_frames() {
    return false;
}

TextureUploadBudget platform_texture_upload_budget() {
    return {SIZE_MAX, 32u << 20, false};
}
bool platform_list_directory(const std::filesystem::path&, std::vector<PlatformFileEntry>&) {
    return false;
}
void platform_begin_file_scan() {}

int platform_run_main(int (*game_main)(int, char**), int argc, char** argv) {
    return game_main(argc, argv);
}

unsigned platform_window_flags(unsigned flags) {
    return flags | window_flag::RESIZABLE;
}

float platform_supersample() {
    return 2.0f;
}

void platform_init_window() {
    ime_ui_set_hint();
    init_window(1280, 720, "YataiDON");
    ime_ui_install();
}

int platform_apply_target_fps(int fps) {
    return fps;
}

PlatformAudioDevice* platform_open_audio(PlatformMixCallback, void*, double&, unsigned long&) {
    return nullptr;
}

void platform_close_audio(PlatformAudioDevice*) {}

std::unique_ptr<PlatformVideoDecoder> platform_open_video_decoder(const std::filesystem::path&,
                                                                  const std::atomic<bool>&) {
    return nullptr;
}

bool platform_take_text_replacement() {
    return false;
}

bool platform_async_text_build() {
    return true;
}

bool platform_file_stamp(const std::filesystem::path&, int64_t&, int64_t&) {
    return false;
}

bool platform_read_file(const char*, std::vector<unsigned char>&) {
    return false;
}

bool platform_resolve_file_path(const std::filesystem::path&, std::filesystem::path&) {
    return false;
}

bool platform_preload_song_previews() {
    return false;
}

bool platform_offers_setting(std::string_view, std::string_view) {
    return true;
}

void platform_load_config(Config&, const toml::table&) {}

void platform_save_config(const Config&, toml::table&) {}
