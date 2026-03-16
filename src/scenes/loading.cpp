#include "loading.h"

void LoadingScreen::on_screen_start() {
    progress_bar_width = tex.screen_width * 0.43;
    progress_bar_height = 50 * tex.screen_scale;
    progress_bar_x = (tex.screen_width - progress_bar_width) / 2;
    progress_bar_y = tex.screen_height * 0.85;

    fade_in = new FadeAnimation(1000, 0.0, false, false, 1.0);
    allnet_indicator = AllNetIcon();

    for (const fs::path& path : global_data.config->paths.tja_path) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.path().extension() == ".tja") {
                songs.push_back(entry.path());
            }
        }
    }
    loading_thread = std::thread(&LoadingScreen::load_song_hashes, this);
}

struct SongCacheEntry {
    std::filesystem::file_time_type mtime;
    std::array<std::string, 5> hashes;
    std::string title;
    std::string subtitle;
};

std::unordered_map<std::string, SongCacheEntry> hash_cache;

void save_hash_cache(const std::unordered_map<std::string, SongCacheEntry>& cache,
                     const std::string& cache_path) {
    std::ofstream f(cache_path, std::ios::binary);
    if (!f) return;

    auto write_string = [&](const std::string& s) {
        size_t len = s.size();
        f.write(reinterpret_cast<const char*>(&len), sizeof(len));
        f.write(s.data(), len);
    };

    size_t count = cache.size();
    f.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& [path, entry] : cache) {
        write_string(path);

        auto mtime_duration = entry.mtime.time_since_epoch().count();
        f.write(reinterpret_cast<const char*>(&mtime_duration), sizeof(mtime_duration));

        for (const auto& h : entry.hashes) {
            write_string(h);
        }

        write_string(entry.title);
        write_string(entry.subtitle);
    }
}

std::unordered_map<std::string, SongCacheEntry> load_hash_cache(const std::string& cache_path) {
    std::unordered_map<std::string, SongCacheEntry> cache;
    std::ifstream f(cache_path, std::ios::binary);
    if (!f) return cache;

    auto read_string = [&](std::string& s) -> bool {
        size_t len = 0;
        if (!f.read(reinterpret_cast<char*>(&len), sizeof(len))) return false;
        if (len > 4096) return false; // sanity cap
        s.resize(len);
        return (bool)f.read(s.data(), len);
    };

    size_t count = 0;
    if (!f.read(reinterpret_cast<char*>(&count), sizeof(count))) return cache;
    if (count > 100000) return cache; // sanity cap

    for (size_t i = 0; i < count; i++) {
        std::string path;
        if (!read_string(path)) break;

        SongCacheEntry entry;

        decltype(entry.mtime.time_since_epoch().count()) mtime_raw = 0;
        if (!f.read(reinterpret_cast<char*>(&mtime_raw), sizeof(mtime_raw))) break;
        entry.mtime = std::filesystem::file_time_type(
            std::filesystem::file_time_type::duration(mtime_raw)
        );

        bool ok = true;
        for (auto& h : entry.hashes) {
            if (!read_string(h)) { ok = false; break; }
        }
        if (!ok) break;

        if (!read_string(entry.title))    break;
        if (!read_string(entry.subtitle)) break;

        cache[path] = std::move(entry);
    }

    return cache;
}

void LoadingScreen::load_song_hashes() {
    const std::string cache_path = "song_cache.bin";
    hash_cache = load_hash_cache(cache_path);
    std::atomic<int> songs_loaded = 0;
    const int thread_count = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    std::mutex scores_mutex;

    auto worker = [&](int start, int end) {
        for (int i = start; i < end; i++) {
            const std::string& path = songs[i].string();
            auto mtime = std::filesystem::last_write_time(path);

            std::array<std::string, 5> hashes;
            std::string title, subtitle;

            {
                std::lock_guard<std::mutex> lock(scores_mutex);
                auto it = hash_cache.find(path);
                if (it != hash_cache.end() && it->second.mtime == mtime) {
                    // Cache hit — skip parsing entirely
                    hashes   = it->second.hashes;
                    title    = it->second.title;
                    subtitle = it->second.subtitle;
                    scores_manager.add_song(hashes, title, subtitle);
                    scores_manager.add_path_binding(path, hashes);
                    progress = (float)++songs_loaded / songs.size();
                    continue;
                }
            }

            // Cache miss — parse and hash normally
            TJAParser parser(path);
            parser.get_metadata();
            for (const auto& [course, course_data] : parser.metadata.course_data) {
                for (int diff = course; diff < 5; diff++) {
                    hashes[diff] = parser.get_diff_hash(diff);
                }
            }

            title    = parser.metadata.title["en"];
            subtitle = parser.metadata.subtitle["en"];

            std::lock_guard<std::mutex> lock(scores_mutex);
            scores_manager.add_song(hashes, title, subtitle);
            scores_manager.add_path_binding(path, hashes);

            // Update cache
            hash_cache[path] = { mtime, hashes, title, subtitle };
            progress = (float)++songs_loaded / songs.size();
        }
    };

    int chunk = songs.size() / thread_count;
    for (int i = 0; i < thread_count; i++) {
        int start = i * chunk;
        int end = (i == thread_count - 1) ? songs.size() : start + chunk;
        threads.emplace_back(worker, start, end);
    }
    for (auto& t : threads) t.join();

    save_hash_cache(hash_cache, cache_path);
    if (fs::exists(fs::path("scores_pytaiko.db"))) {
        scores_manager.py_taiko_import(fs::path("scores_pytaiko.db"));
        fs::remove(fs::path("scores_pytaiko.db"));
    }
    loading_complete = true;
}

Screens LoadingScreen::on_screen_end(Screens next_screen) {
    if (loading_thread.joinable()) {
        loading_thread.join();
    }
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> LoadingScreen::update() {
    Screen::update();

    if (loading_complete && !fade_in->isStarted()) {
        fade_in->start();
    }

    fade_in->update(get_current_ms());
    if (fade_in->is_finished) {
        return on_screen_end(Screens::TITLE);
    }

    return std::nullopt;
}

void LoadingScreen::draw() {
    ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::BLACK);
    tex.draw_texture("kidou", "warning");

    ray::DrawRectangle(progress_bar_x, progress_bar_y, progress_bar_width, progress_bar_height, ray::Color(101, 0, 0, 255));

    float clamped_progress = std::max(0.0f, std::min(1.0f, progress.load()));
    float fill_width = progress_bar_width * clamped_progress;
    if (fill_width > 0) {
        ray::DrawRectangle(progress_bar_x, progress_bar_y, fill_width, progress_bar_height, ray::RED);
    }

    ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::Fade(ray::WHITE, fade_in->attribute));
    allnet_indicator.draw();
}
