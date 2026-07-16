#include "loading.h"
#include "../libs/global_data.h"
#include "../libs/scores.h"
#include "../libs/filesystem.h"
#include "../libs/song_parser.h"
#include "../objects/song_select/file_navigator/navigator.h"

void LoadingScreen::on_screen_start() {
    Screen::on_screen_start();
    progress_bar_width = tex.screen_width * 0.43;
    progress_bar_height = 50 * tex.screen_scale;
    progress_bar_x = (tex.screen_width - progress_bar_width) / 2;
    progress_bar_y = tex.screen_height * 0.85;

    fade_in = std::make_unique<FadeAnimation>(1000, 0.0, false, false, 1.0);
    allnet_indicator = AllNetIcon();

    songs = get_song_files(global_data.config->paths.tja_path);
#ifdef __EMSCRIPTEN__
    load_song_hashes();
#else
    loading_thread = std::thread(&LoadingScreen::load_song_hashes, this);
#endif
}

void LoadingScreen::load_song_hashes() {
    std::atomic<int> songs_loaded = 0;
    const int thread_count = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    std::mutex scores_mutex;

    auto worker = [&](int start, int end) {
        for (int i = start; i < end; i++) {
            auto u8 = songs[i].u8string();
            const std::string path(u8.begin(), u8.end());

            std::error_code ec;
            auto mtime = std::filesystem::last_write_time(songs[i], ec);
            if (ec) {
                spdlog::error("Could not stat {}: {}", path, ec.message());
                continue;
            }

            std::array<std::string, 5> hashes;
            std::string title, subtitle;

            try {
                SongParser parser(songs[i]);
                spdlog::debug("Parsing song: {}", path);
                if (songs[i].extension() == ".osu") {
                    hashes[0] = parser.get_diff_hash(0);
                } else {
                    for (const auto& [course, course_data] : parser.metadata.course_data) {
                        if (course < 0 || course >= static_cast<int>(hashes.size()))
                            continue;
                        hashes[course] = parser.get_diff_hash(course);
                    }
                }
                title    = parser.metadata.title.count("en") ? parser.metadata.title.at("en") : "";
                subtitle = parser.metadata.subtitle.count("en") ? parser.metadata.subtitle.at("en") : "";
            } catch (const std::exception& e) {
                spdlog::error("Failed to parse song {}: {}", path, e.what());
                continue;
            }

            std::lock_guard<std::mutex> lock(scores_mutex);
            scores_manager.add_song(hashes, title, subtitle);
            scores_manager.add_path_binding(songs[i], hashes);

            progress = (float)++songs_loaded / songs.size();
        }
    };

    scores_manager.begin_transaction();
#ifdef __EMSCRIPTEN__
    worker(0, songs.size());
#else
    int chunk = songs.size() / thread_count;
    for (int i = 0; i < thread_count; i++) {
        int start = i * chunk;
        int end = (i == thread_count - 1) ? songs.size() : start + chunk;
        threads.emplace_back(worker, start, end);
    }
    for (auto& t : threads) t.join();
#endif
    scores_manager.commit();

    if (fs::exists(fs::path("scores_pytaiko.db"))) {
        scores_manager.py_taiko_import(fs::path("scores_pytaiko.db"));
        fs::remove(fs::path("scores_pytaiko.db"));
    }

    load_navigator();
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
    allnet_indicator.update(get_current_ms());

    if (loading_complete && !fade_in->isStarted()) {
        fade_in->start();
    }

    fade_in->update(get_current_ms());
    if (fade_in->is_finished) {
#ifdef __EMSCRIPTEN__
        return on_screen_end(Screens::ENTRY);
#else
        return on_screen_end(Screens::TITLE);
#endif
    }

    return std::nullopt;
}

void LoadingScreen::draw() {
    ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::BLACK);

    ray::DrawRectangle(progress_bar_x, progress_bar_y, progress_bar_width, progress_bar_height, ray::Color(101, 0, 0, 255));

    float clamped_progress = std::max(0.0f, std::min(1.0f, progress.load()));
    float fill_width = progress_bar_width * clamped_progress;
    if (fill_width > 0) {
        ray::DrawRectangle(progress_bar_x, progress_bar_y, fill_width, progress_bar_height, ray::RED);
    }
    tex.draw_texture(KIDOU::WARNING);

    ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::Fade(ray::WHITE, fade_in->attribute));
    allnet_indicator.draw();
}
