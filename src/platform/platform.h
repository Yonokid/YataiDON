#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

// Services every platform provides. CMake compiles exactly one platform's sources,
// so callers include this header and call these without platform guards.

// Prepares the directory the game runs from (assets, config, scores) and returns it.
// An empty path keeps the current working directory.
std::filesystem::path platform_get_working_directory();

// Installs signal/exception handlers that report the crash before exiting.
void platform_install_crash_handlers();

// Destroys the main window, working around platform driver teardown issues.
void platform_close_window();

// Runs once the window exists (e.g. hooking mobile app lifecycle events).
void platform_initialize_after_window();

// Polls native input on the main thread before SDL events are published.
void platform_poll_input();

// Gives a persistent animation worker a platform-appropriate scheduling preference.
void platform_initialize_animation_worker();

// Monotonic milliseconds since an arbitrary epoch, frozen while the app is
// suspended (backgrounded on iOS/Android). Safe to call from any thread.
double platform_game_time_ms();

// True while the OS has the app backgrounded; frames should be skipped.
bool platform_is_suspended();

// Runs the game's main; a platform can wrap it to report errors that escape it.
int platform_run_main(int (*game_main)(int, char**), int argc, char** argv);

// Adjusts the window flags (gfx.h window_flag) before the window is created.
unsigned platform_window_flags(unsigned flags);

// Supersampling factor for offscreen 3D character rendering before its FXAA pass
// (2 on most platforms; 1 where the GPU has no headroom at native resolution).
float platform_supersample();

// Creates the main window.
void platform_init_window();

// Requests a target frame rate (-1 = unlimited) and returns the one in effect,
// e.g. the output refresh a console display actually switched to.
int platform_apply_target_fps(int fps);

bool platform_present_paces_frames();
bool platform_threaded_renderer();

// A platform's own audio output, used in place of SDL/RtAudio where it has one. The mix
// callback fills `frames` interleaved stereo floats. Returns null when the platform has none;
// on success `sample_rate` and `frames` hold what the device actually runs at.
struct PlatformAudioDevice;
using PlatformMixCallback = void (*)(float* out, unsigned int frames, void* context);
PlatformAudioDevice* platform_open_audio(PlatformMixCallback mix, void* context, double& sample_rate,
                                         unsigned long& frames);
void platform_close_audio(PlatformAudioDevice* device);

// A platform's hardware video decoder. next() fills the next RGBA frame (false at the end)
// and throws on failure, after which the caller continues in software.
class PlatformVideoDecoder {
public:
    virtual ~PlatformVideoDecoder() = default;
    virtual bool next(std::vector<uint8_t>& rgba, int& width, int& height) = 0;
};
// Null when the platform has none or cannot decode `path`: the software decoder is used.
std::unique_ptr<PlatformVideoDecoder> platform_open_video_decoder(const std::filesystem::path& path,
                                                                  const std::atomic<bool>& stop);

// True once after a native on-screen keyboard dialog returned its finished text: the edited
// field is replaced rather than appended to.
bool platform_take_text_replacement();

// False where a screen full of per-label worker threads exceeds native resources.
bool platform_async_text_build();

// Supplies a cache timestamp and size without repeated portable filesystem queries.
bool platform_file_stamp(const std::filesystem::path& path, int64_t& mtime, int64_t& size);

struct PlatformFileEntry {
    std::filesystem::path path;
    bool directory;
    int64_t size;
};
// Discards native metadata from a previous chart scan before the next walk.
void platform_begin_file_scan();
// Entries have no symlinks. False leaves traversal to the portable implementation.
bool platform_list_directory(const std::filesystem::path& path, std::vector<PlatformFileEntry>& entries);

// Reads a whole file through a faster native path where the platform has one; false means
// "no native path", and the caller uses the portable reader.
bool platform_read_file(const char* path, std::vector<unsigned char>& out);

// Resolves native device paths that the portable filesystem cannot canonicalize.
// False keeps portable symlink resolution in the caller.
bool platform_resolve_file_path(const std::filesystem::path& path, std::filesystem::path& resolved);

// Whether the settings screen offers an option (empty value) or one of its values,
// e.g. no window-mode options on a console.
bool platform_offers_setting(std::string_view path, std::string_view value);

// True where streaming a song preview from disk would stall the UI (slow storage): previews
// are decoded whole on a worker thread instead.
bool platform_preload_song_previews();

// True when memory allows keeping decoded/uploaded skin assets after a screen unloads
// (see asset_cache.h), so revisiting a screen skips disk reads and decoding.
bool platform_retains_assets();
