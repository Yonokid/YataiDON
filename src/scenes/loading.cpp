#include "loading.h"

#include <unordered_set>

#include "../libs/file/filesystem.h"
#include "../libs/file/scores.h"
#include "../libs/file/script.h"
#include "../libs/global_data.h"
#include "../libs/input/input.h"
#include "../libs/network/skin_updater.h"
#include "../libs/parsers/song_parser.h"
#include "../libs/song_library.h"
#include "../libs/song_library_loader.h"
#include "../objects/song_select/file_navigator/navigator.h"
#include "../objects/song_select/song_select_script.h"
#include "../platform/platform.h"

void LoadingScreen::on_screen_start() {
    init_visuals();

    start_ms = get_current_ms();
    const bool populate_library = get_song_library().begin_loading();
    try {
        loading_thread = std::thread(&LoadingScreen::load_song_hashes, this, populate_library);
    } catch (...) {
        if (populate_library) {
            get_song_library().fail_loading();
        }

        throw;
    }
}

// Loads this screen's textures/sounds and (re)grabs the texture pointers and
// geometry from the current skin. Split out of on_screen_start() so it can be
// run again after a skin update forces a skin reload.
void LoadingScreen::init_visuals() {
    tex.load_screen_textures("loading");
    audio.load_screen_sounds("loading");

    progress_bar.width = tex.screen_width * 0.43;
    progress_bar.height = 50 * tex.screen_scale;
    progress_bar.x = (tex.screen_width - progress_bar.width) / 2;
    progress_bar.y = tex.screen_height * 0.85;

    if (const SkinInfo* bar = tex.get_skin_config(SC::LOADING_PROGRESS_BAR); bar && bar->width > 0 && bar->height > 0) {
        progress_bar = {bar->x, bar->y, bar->width, bar->height};
    }

    double fade_ms = 1000;
    if (const SkinInfo* f = tex.get_skin_config(SC::LOADING_FADE_MS); f && f->x > 0) {
        fade_ms = f->x;
    }
    fade_in = std::make_unique<FadeAnimation>(fade_ms, 0.0, false, false, 1.0);
    allnet_indicator.emplace();
    t_warning = tex.get_texture("kidou/warning");

    countdown_ms = 0.0;
    if (tex.options[SCO::LOADING_COUNTDOWN] && script_manager.lua) {
        sol::protected_function require = (*script_manager.lua)["require"];
        auto r = require("loading/loading_countdown");
        if (r.valid()) {
            sol::table module = r;
            countdown_ms = module.get_or("seconds", 0.0) * 1000.0;
        }
    }
    has_countdown = countdown_ms > 0.0;
}

void LoadingScreen::load_song_hashes(bool populate_library) {
    try {
        if (!populate_library) {
            const std::atomic<bool> cancel{false};
            get_song_library().wait(cancel);
            populate_library = get_song_library().begin_loading();
        }

        // Listed here, not in on_screen_start: the screen keeps drawing while a large library is walked
        songs = get_song_files(global_data.config->paths.tja_path);
        std::atomic<int> songs_loaded = 0;
        const unsigned thread_count = std::clamp(std::thread::hardware_concurrency(), 2u, 8u);
        std::vector<std::jthread> threads;
        std::mutex scores_mutex;

        // Files whose size and modification time match the cache are not parsed again; the
        // workers only read `cache`, new or changed entries are written after they finish.
        const std::unordered_map<std::string, SongCacheEntry> cache = scores_manager.load_song_cache();
        std::vector<std::pair<std::string, SongCacheEntry>> fresh;
        std::atomic<int> cache_hits = 0;

        std::atomic<size_t> next_song = 0;
        auto worker = [&] {
            for (size_t i; (i = next_song.fetch_add(1)) < songs.size();) {
                auto u8 = songs[i].u8string();
                const std::string path(u8.begin(), u8.end());

                std::error_code ec;
                SongCacheEntry stamp;
                if (!platform_file_stamp(songs[i], stamp.mtime, stamp.size)) {
                    const auto mtime = std::filesystem::last_write_time(songs[i], ec);
                    if (ec) {
                        spdlog::error("Could not stat {}: {}", path, ec.message());
                        continue;
                    }
                    const auto size = std::filesystem::file_size(songs[i], ec);
                    stamp.mtime = static_cast<int64_t>(mtime.time_since_epoch().count());
                    stamp.size = ec ? -1 : static_cast<int64_t>(size);
                }

                auto cached = cache.find(path);
                const bool from_cache =
                    cached != cache.end() && cached->second.mtime == stamp.mtime && cached->second.size == stamp.size;
                if (from_cache) {
                    cache_hits++;
                } else {
                    try {
                        SongParser parser(songs[i]);
                        spdlog::debug("Parsing song: {}", path);
                        if (songs[i].extension() == ".osu") {
                            stamp.hashes[0] = parser.get_diff_hash(0);
                        } else {
                            for (const auto& [course, course_data] : parser.metadata.course_data) {
                                if (course < 0 || course >= static_cast<int>(stamp.hashes.size())) {
                                    continue;
                                }
                                stamp.hashes[course] = parser.get_diff_hash(course);
                            }
                        }
                        auto metadata = song_metadata_from(parser.metadata);
                        stamp.title = std::move(metadata.title);
                        stamp.subtitle = std::move(metadata.subtitle);
                        stamp.titles = std::move(metadata.titles);
                        stamp.subtitles = std::move(metadata.subtitles);
                        stamp.levels = metadata.levels;
                    } catch (const std::exception& e) {
                        spdlog::error("Failed to parse song {}: {}", path, e.what());
                        continue;
                    }
                }

                try {
                    std::scoped_lock lock(scores_mutex);
                    const SongCacheEntry& meta = from_cache ? cached->second : stamp;
                    // a cached file's songs row was written when it was parsed
                    if (!from_cache) {
                        scores_manager.add_song(meta.hashes, meta.title, meta.subtitle);
                    }
                    scores_manager.add_path_binding(songs[i], meta.hashes);
                    if (populate_library && is_library_song_file(songs[i])) {
                        get_song_library().add(songs[i], SongMetadata{meta.title, meta.subtitle, meta.titles,
                                                                      meta.subtitles, meta.levels});
                    }
                    if (!from_cache) {
                        fresh.emplace_back(path, std::move(stamp));
                    }

                    progress = static_cast<float>(++songs_loaded) / songs.size();
                } catch (const std::exception& e) {
                    spdlog::error("Failed to record song {}: {}", path, e.what());
                }
            }
        };

        scores_manager.begin_transaction();
        for (unsigned i = 0; i < thread_count && i < songs.size(); i++) {
            threads.emplace_back(worker);
        }
        for (auto& t : threads) {
            t.join();
        }

        for (const auto& [path, entry] : fresh) {
            scores_manager.store_song_cache(path, entry);
        }
        // forget files that are gone, so the table does not grow with deleted songs
        if (!cache.empty()) {
            std::unordered_set<std::string> present;
            present.reserve(songs.size());
            for (const auto& song : songs) {
                auto u8 = song.u8string();
                present.emplace(u8.begin(), u8.end());
            }
            for (const auto& [path, entry] : cache) {
                if (!present.contains(path)) {
                    scores_manager.remove_song_cache(path);
                }
            }
        }
        scores_manager.commit();
        spdlog::info("Song scan: {} files, {} from cache, {} parsed", songs.size(), cache_hits.load(), fresh.size());

        if (fs::exists(fs::path("scores_pytaiko.db"))) {
            scores_manager.py_taiko_import(fs::path("scores_pytaiko.db"));
            fs::remove(fs::path("scores_pytaiko.db"));
        }

        if (populate_library) {
            get_song_library().finish_loading();
        }

        load_navigator();
    } catch (const std::exception& e) {
        if (populate_library) {
            get_song_library().fail_loading();
        }

        spdlog::error("Loading failed: {}", e.what());
    }
    loading_complete = true;
}

void LoadingScreen::load_navigator() {
    navigator.preload(global_data.config->paths.tja_path);
}

Screens LoadingScreen::on_screen_end(Screens next_screen) {
    if (loading_thread.joinable()) {
        loading_thread.join();
    }
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> LoadingScreen::update() {
    Screen::update();
    if (allnet_indicator) {
        allnet_indicator->update(get_current_ms());
    }

    if (is_l_don_pressed() || is_r_don_pressed()) {
        skip_requested = true;
    }

    if (!skin_reloaded && loading_complete && skin_updater.finished() && skin_updater.updated_this_pass()) {
        skin_reloaded = true;
        spdlog::info("Skin update applied; reloading skin before leaving the loading screen");
        try {
            allnet_indicator.reset();
            t_warning = nullptr;
            has_countdown = false;
            drop_other_screens_for_skin_reload();
            navigator.reset_for_skin_reload();
            unload_skin();
            load_skin();
            reload_skin_screens();
        } catch (const std::exception& e) {
            spdlog::error("Skin reload after update failed: {}", e.what());
        }
        init_visuals();
    }

    const bool skin_updater_blocking = skin_updater.applying_updates();

    if (loading_complete && !fade_in->isStarted() && !skin_updater_blocking &&
        (skip_requested || get_current_ms() - start_ms >= countdown_ms)) {
        fade_in->start();
    }

    fade_in->update(get_current_ms());
    if (fade_in->is_finished) {
        if (global_data.needs_onboarding) {
            return on_screen_end(Screens::ONBOARDING);
        }
#ifdef __EMSCRIPTEN__
        return on_screen_end(Screens::ENTRY);
#else
        return on_screen_end(Screens::TITLE);
#endif
    }

    return std::nullopt;
}

void LoadingScreen::draw() {
    draw_rectangle(0, 0, tex.screen_width, tex.screen_height, Color::BLACK);

    draw_rectangle(progress_bar, Color(101, 0, 0, 255));

    float clamped_progress = std::max(0.0f, std::min(1.0f, progress.load()));
    Rect fill = progress_bar;
    fill.width *= clamped_progress;
    if (fill.width > 0) {
        draw_rectangle(fill, Color::RED);
    }
    tex.draw_texture(t_warning);

    if (has_countdown) {
        sol::protected_function draw_countdown =
            cached_skin_module_fn("loading/loading_countdown", "draw", "__loading_countdown_draw");
        if (draw_countdown.valid()) {
            auto r = draw_countdown(std::max(0.0, countdown_ms - (get_current_ms() - start_ms)));
            if (!r.valid()) {
                sol::error err = r;
                spdlog::error("Lua error in loading_countdown.draw: {}", err.what());
            }
        }
    }

    draw_rectangle(0, 0, tex.screen_width, tex.screen_height, fade(Color::WHITE, fade_in->attribute));
    if (allnet_indicator) {
        allnet_indicator->draw();
    }
}
