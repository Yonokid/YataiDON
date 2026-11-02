#include "song_library_loader.h"

#include <algorithm>

#include "file/filesystem.h"
#include "parsers/song_parser.h"

bool is_library_song_file(const fs::path& path) {
    if (!fs::is_regular_file(path)) {
        return false;
    }

    const auto extension = path.extension();
    return extension == ".tja" || extension == ".osu";
}

SongMetadata song_metadata_from(const TJAMetadata& metadata) {
    SongMetadata result;
    if (auto it = metadata.title.find("en"); it != metadata.title.end()) {
        result.title = it->second;
    }

    if (auto it = metadata.subtitle.find("en"); it != metadata.subtitle.end()) {
        result.subtitle = it->second;
    }

    for (const auto& [lang, title] : metadata.title) {
        result.titles.emplace_back(lang, title);
    }

    for (const auto& [lang, subtitle] : metadata.subtitle) {
        result.subtitles.emplace_back(lang, subtitle);
    }

    for (const auto& [course, data] : metadata.course_data) {
        if (course >= 0 && course <= 4) {
            result.levels[course] = static_cast<int>(data.level);
        }
    }

    return result;
}

SongLibraryLoader::SongLibraryLoader(SongLibrary& library) : library(library) {}

SongLibraryLoader::~SongLibraryLoader() {
    stop();
}

void SongLibraryLoader::start(const std::vector<fs::path>& roots) {
    std::scoped_lock lock(lifecycle_mutex);
    if (library.is_loaded() || library.is_loading()) {
        return;
    }

    if (worker.joinable()) {
        worker.join();
    }

    if (!library.begin_loading()) {
        return;
    }

    stopping = false;
    try {
        auto scan = [this, roots] {
            try {
                load(roots);
            } catch (const std::exception& e) {
                library.fail_loading();
                spdlog::error("Song library scan failed: {}", e.what());
            }
        };

        worker = std::thread(std::move(scan));
    } catch (...) {
        library.fail_loading();
        throw;
    }
}

void SongLibraryLoader::load(const std::vector<fs::path>& roots) {
    const auto files = get_song_files(roots);
    std::atomic<size_t> cursor{0};
    auto parse = [&] {
        for (;;) {
            const size_t i = cursor.fetch_add(1);
            if (i >= files.size() || stopping) {
                break;
            }

            try {
                if (!is_library_song_file(files[i])) {
                    continue;
                }

                SongParser parser(files[i]);
                library.add(files[i], song_metadata_from(parser.metadata));
            } catch (const std::exception& e) {
                spdlog::warn("Skipping song during scan: {}", e.what());
            }
        }
    };

    {
        std::vector<std::jthread> pool;
        const size_t workers =
            std::min(files.size(), static_cast<size_t>(std::max(1u, std::thread::hardware_concurrency())));
        pool.reserve(workers);
        for (size_t i = 0; i < workers; ++i) {
            pool.emplace_back(parse);
        }
    }

    if (stopping) {
        library.fail_loading();
    } else {
        library.finish_loading();
    }
}

void SongLibraryLoader::stop() {
    std::scoped_lock lock(lifecycle_mutex);
    stopping = true;
    if (worker.joinable()) {
        worker.join();
    }
}

SongLibraryLoader& get_song_library_loader() {
    static SongLibraryLoader loader(get_song_library());
    return loader;
}
