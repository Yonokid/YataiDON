#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <string>
#include <switch.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>

#include "../../platform/switch/controller-layout.h"
#include "../../platform/switch/input.h"
#include "../../platform/switch/keyboard.h"
#include "../../platform/switch/sqlite-vfs.h"
#include "../libs/file/config.h"
#include "../libs/gfx.h"
#include "../libs/gfx/sdl_compat.h"
#include "../libs/input/ime_ui.h"
#include "platform.h"

namespace {
    // A fresh scan owns this snapshot; timestamps are queried while the directory
    // entries are hot in the filesystem cache, not after walking the entire SD tree.
    class SwitchChartIndex {
        struct Stamp {
            int64_t mtime;
            int64_t size;
        };
        std::mutex mutex;
        std::unordered_map<std::string, Stamp> entries;

    public:
        void clear() {
            std::scoped_lock lock(mutex);
            entries.clear();
        }

        void put(std::string path, int64_t mtime, int64_t size) {
            std::scoped_lock lock(mutex);
            entries.insert_or_assign(std::move(path), Stamp{mtime, size});
        }

        bool get(const std::string& path, int64_t& mtime, int64_t& size) {
            std::scoped_lock lock(mutex);
            auto found = entries.find(path);
            if (found == entries.end()) {
                return false;
            }
            mtime = found->second.mtime;
            size = found->second.size;
            return true;
        }
    };

    class SwitchTimestampCache {
        std::mutex mutex;
        std::unordered_map<int64_t, int64_t> values;

    public:
        template<class Convert>
        int64_t get(int64_t timestamp, Convert&& convert) {
            std::scoped_lock lock(mutex);
            auto found = values.find(timestamp);
            if (found != values.end()) {
                return found->second;
            }
            const int64_t result = convert(timestamp);
            values.emplace(timestamp, result);
            return result;
        }
    };

    SwitchChartIndex chart_index;
    SwitchTimestampCache timestamp_cache;

    int64_t cache_timestamp(int64_t modified) {
        return timestamp_cache.get(modified, [](int64_t raw) {
            time_t seconds = static_cast<time_t>(raw);
            tm calendar{};
            if (gmtime_r(&seconds, &calendar)) {
                // Match fsdev's FAT timestamp conversion so existing chart caches remain valid.
                TimeCalendarTime local{};
                local.year = calendar.tm_year + 1900;
                local.month = calendar.tm_mon + 1;
                local.day = calendar.tm_mday;
                local.hour = calendar.tm_hour;
                local.minute = calendar.tm_min;
                local.second = calendar.tm_sec;
                u64 utc = 0;
                if (R_SUCCEEDED(timeToPosixTimeWithMyRule(&local, &utc, 1, nullptr))) {
                    seconds = static_cast<time_t>(utc);
                }
            }
            const auto system_time = std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
            return std::filesystem::file_time_type::clock::from_sys(system_time).time_since_epoch().count();
        });
    }

    bool query_timestamp(FsFileSystem* device, const char* native_path, int64_t& mtime) {
        FsTimeStampRaw timestamp{};
        if (R_FAILED(fsFsGetFileTimeStampRaw(device, native_path, &timestamp)) || !timestamp.is_valid) {
            return false;
        }
        mtime = cache_timestamp(timestamp.modified);
        return true;
    }
} // namespace

void platform_begin_file_scan() {
    chart_index.clear();
}

bool platform_present_paces_frames() {
    return true;
}
bool platform_threaded_renderer() {
    return true;
}

bool platform_list_directory(const std::filesystem::path& path, std::vector<PlatformFileEntry>& entries) {
    FsFileSystem* device = nullptr;
    char native_path[FS_MAX_PATH];
    if (fsdevTranslatePath(path.string().c_str(), &device, native_path) != 0) {
        return false;
    }
    FsDir directory;
    if (R_FAILED(
            fsFsOpenDirectory(device, native_path, FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &directory))) {
        return false;
    }
    std::vector<PlatformFileEntry> result;
    const size_t native_length = std::strlen(native_path);
    const char* separator = native_length > 0 && native_path[native_length - 1] == '/' ? "" : "/";
    std::array<FsDirectoryEntry, 32> batch;
    try {
        for (;;) {
            s64 count = 0;
            if (R_FAILED(fsDirRead(&directory, &count, batch.size(), batch.data())) || count < 0) {
                fsDirClose(&directory);
                return false;
            }
            if (count == 0) {
                break;
            }
            for (s64 i = 0; i < count; ++i) {
                const auto& entry = batch[i];
                if (std::strcmp(entry.name, ".") == 0 || std::strcmp(entry.name, "..") == 0) {
                    continue;
                }
                result.push_back({path / entry.name, entry.type == FsDirEntryType_Dir, entry.file_size});
                const auto& file = result.back();
                const auto extension = file.path.extension();
                if (!file.directory && (extension == ".tja" || extension == ".osu" || extension == ".bin")) {
                    char file_path[FS_MAX_PATH]{};
                    const int length =
                        std::snprintf(file_path, sizeof(file_path), "%s%s%s", native_path, separator, entry.name);
                    int64_t mtime = 0;
                    if (length >= 0 && static_cast<size_t>(length) < sizeof(file_path) &&
                        query_timestamp(device, file_path, mtime)) {
                        chart_index.put(file.path.string(), mtime, file.size);
                    }
                }
            }
        }
    } catch (...) {
        fsDirClose(&directory);
        throw;
    }
    fsDirClose(&directory);
    entries = std::move(result);
    return true;
}

std::filesystem::path platform_get_working_directory() {
    const std::filesystem::path root = "sdmc:/switch/YataiDON";
    std::filesystem::create_directories(root);
    return root;
}

bool platform_resolve_file_path(const std::filesystem::path& path, std::filesystem::path& resolved) {
    FsFileSystem* device = nullptr;
    char native_path[FS_MAX_PATH];
    const auto source = path.string();
    if (fsdevTranslatePath(source.c_str(), &device, native_path) != 0) {
        return false;
    }
    const auto device_path = source.contains(':') ? source : std::filesystem::current_path().string();
    const auto colon = device_path.find(':');
    if (colon == std::string::npos) {
        return false;
    }
    // fsdev mounts have no symlinks; retain the device prefix instead of using POSIX canonical().
    const auto normalized = std::filesystem::path(native_path).lexically_normal();
    resolved = std::filesystem::path(device_path.substr(0, colon + 1) + normalized.generic_string());
    return true;
}

void platform_install_crash_handlers() {}

int platform_run_main(int (*game_main)(int, char**), int argc, char** argv) {
    int result = 1;
    try {
        // Mesa's shader cache must live on writable storage before Vulkan is initialized.
        const auto root = platform_get_working_directory();
        const auto cache = root / "cache/mesa";
        std::filesystem::create_directories(cache);
        // A terminated homebrew process cannot leave a live SQLite owner.
        // rmdir only removes empty lock directories, never database contents.
        for (const char* name : {"scores.db.lock", "scores_gen3.db.lock", "scores_pytaiko.db.lock"}) {
            rmdir((root / name).c_str());
        }
        if (setenv("MESA_SHADER_CACHE_DIR", cache.string().c_str(), 1) != 0) {
            throw std::runtime_error("Could not configure the Mesa shader cache");
        }
        if (switch_sqlite_initialize() != 0) {
            throw std::runtime_error("Could not initialize SQLite's SD path adapter");
        }
        SDL_SetMainReady();
        result = game_main(argc, argv);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Startup exception: %s\n", error.what());
    } catch (...) {
        std::fprintf(stderr, "Startup exception: unknown\n");
    }
    yataidon_switch_close_controllers();
    SDL_Quit();
    return result;
}

void platform_init_window() {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "Switch");
    if (!SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)) {
        throw std::runtime_error(SDL_GetError());
    }
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        char guid[33];
        char mapping[512];
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof(guid));
        std::snprintf(mapping, sizeof(mapping), "%s,%s", guid, switch_controls::mapping);
        if (SDL_GameControllerAddMapping(mapping) < 0) {
            throw std::runtime_error(SDL_GetError());
        }
    }
    ime_ui_set_hint();
    init_window(1280, 720, "YataiDON");
    ime_ui_install();
}

void platform_close_window() {
    close_window();
}

namespace {
    void prefer_cpu(int core) {
        u64 mask = 0;
        if (R_SUCCEEDED(svcGetInfo(&mask, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0)) && (mask & (u64{1} << core))) {
            svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, mask);
        }
    }
} // namespace

void platform_initialize_after_window() {
    prefer_cpu(0);
}

void platform_initialize_animation_worker() {
    prefer_cpu(2);
}

void platform_poll_input() {
    yataidon_switch_poll_controllers();
    switch_poll_keyboard();
}

unsigned platform_window_flags(unsigned flags) {
    return (flags & ~window_flag::RESIZABLE) | window_flag::VSYNC;
}

float platform_supersample() {
    return 1.0f;
}

int platform_apply_target_fps(int) {
    return 60;
}

double platform_game_time_ms() {
    return static_cast<double>(armTicksToNs(armGetSystemTick())) / 1000000.0;
}

bool platform_is_suspended() {
    // SDL's Switch event pump owns applet lifecycle handling.
    return false;
}

PlatformAudioDevice* platform_open_audio(PlatformMixCallback, void*, double&, unsigned long&) {
    return nullptr; // use SDL's libnx audio output
}

void platform_close_audio(PlatformAudioDevice*) {}

std::unique_ptr<PlatformVideoDecoder> platform_open_video_decoder(const std::filesystem::path&,
                                                                  const std::atomic<bool>&) {
    return nullptr;
}

bool platform_take_text_replacement() {
    return switch_take_text_replacement();
}

bool platform_async_text_build() {
    return false;
}

bool platform_file_stamp(const std::filesystem::path& path, int64_t& mtime, int64_t& size) {
    if (chart_index.get(path.string(), mtime, size)) {
        return true;
    }
    FsFileSystem* device = nullptr;
    char native_path[FS_MAX_PATH];
    if (fsdevTranslatePath(path.string().c_str(), &device, native_path) != 0) {
        return false;
    }
    if (!query_timestamp(device, native_path, mtime)) {
        return false;
    }
    s64 bytes = 0;
    FsFile file;
    if (R_FAILED(fsFsOpenFile(device, native_path, FsOpenMode_Read, &file))) {
        return false;
    }
    const Result result = fsFileGetSize(&file, &bytes);
    fsFileClose(&file);
    if (R_FAILED(result)) {
        return false;
    }
    size = bytes;
    return true;
}

bool platform_read_file(const char* path, std::vector<unsigned char>& out) {
    FsFileSystem* device = nullptr;
    char native_path[FS_MAX_PATH];
    if (fsdevTranslatePath(path, &device, native_path) != 0) {
        return false;
    }
    FsFile file;
    if (R_FAILED(fsFsOpenFile(device, native_path, FsOpenMode_Read, &file))) {
        return false;
    }
    s64 size = 0;
    Result result = fsFileGetSize(&file, &size);
    if (R_FAILED(result) || size < 0 || static_cast<u64>(size) > out.max_size()) {
        fsFileClose(&file);
        return false;
    }
    try {
        out.resize(static_cast<size_t>(size));
    } catch (...) {
        fsFileClose(&file);
        throw;
    }
    u64 offset = 0;
    while (offset < static_cast<u64>(size)) {
        u64 read = 0;
        const u64 count = std::min<u64>(size - offset, 1024 * 1024);
        result = fsFileRead(&file, offset, out.data() + offset, count, FsReadOption_None, &read);
        if (R_FAILED(result) || read != count) {
            fsFileClose(&file);
            out.clear();
            return false;
        }
        offset += read;
    }
    fsFileClose(&file);
    return true;
}

bool platform_preload_song_previews() {
    return true;
}

bool platform_retains_assets() {
    return true;
}

bool platform_offers_setting(std::string_view path, std::string_view value) {
    if (path == "audio/sample_rate") {
        return value.empty() || value == "48000";
    }
    if (path == "audio/buffer_size" && !value.empty()) {
        int frames = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), frames);
        return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && frames >= 1024;
    }
    if (path == "video/vsync" || path == "video/fullscreen" || path == "video/borderless" ||
        path == "keys/fullscreen_key" || path == "keys/borderless_key" || path == "audio/device" ||
        path == "audio/device_type") {
        return false;
    }
    return path != "video/target_fps" || value.empty() || value == "60";
}

void platform_load_config(Config& config, const toml::table& file) {
    config.keys.gamepad_menu_layout = file["keys"]["gamepad_menu_layout"].value_or(true);
    const int layout_version = file["switch"]["controller_layout_version"].value_or(0);
    if (layout_version < 2) {
        switch_controls::upgrade_layout(config.gamepad_1p);
        switch_controls::upgrade_layout(config.gamepad_2p);
        config.keys.gamepad_menu_layout = true;
    }
    config.video.vsync = true;
    config.video.fullscreen = config.video.borderless = false;
    config.video.target_fps = 60;
    config.audio.sample_rate = 48000;
    config.audio.buffer_size = std::max(config.audio.buffer_size, 1024);
}

void platform_save_config(const Config&, toml::table& out) {
    out.insert("switch", toml::table{{"controller_layout_version", 2}});
}
