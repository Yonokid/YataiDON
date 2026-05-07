#include "filesystem.h"
#include "miniz/miniz.h"
#include "scores.h"
#include "song_parser.h"
#include <fstream>
#include <spdlog/spdlog.h>

void save_hash_cache(const SongCache& cache, const fs::path& cache_path) {
    std::ofstream f(cache_path, std::ios::binary);
    if (!f.is_open()) {
        spdlog::error("failed to write to cache file: " + cache_path.string());
        return;
    }

    f.write(reinterpret_cast<const char*>(&CACHE_MAGIC),   sizeof(CACHE_MAGIC));
    f.write(reinterpret_cast<const char*>(&CACHE_VERSION), sizeof(CACHE_VERSION));

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

// Returns {cache_version, cache_entries}. Version 1 = old format (no header).
std::pair<int, SongCache> load_hash_cache(const fs::path& cache_path) {
    std::unordered_map<std::string, SongCacheEntry> cache;
    std::ifstream f(cache_path, std::ios::binary);
    if (!f.is_open()) {
        if (fs::exists(cache_path))
            spdlog::error("failed to read cache file: {}", cache_path.string());
        return {0, cache};
    }

    int version = 1;
    uint32_t first4 = 0;
    if (f.read(reinterpret_cast<char*>(&first4), sizeof(first4))) {
        if (first4 == CACHE_MAGIC) {
            uint32_t v = 0;
            f.read(reinterpret_cast<char*>(&v), sizeof(v));
            version = static_cast<int>(v);
        } else {
            f.seekg(0); // old format: first bytes are the count, seek back
        }
    }

    auto read_string = [&](std::string& s) -> bool {
        size_t len = 0;
        if (!f.read(reinterpret_cast<char*>(&len), sizeof(len))) return false;
        if (len > 4096) return false; // sanity cap
        s.resize(len);
        return (bool)f.read(s.data(), len);
    };

    size_t count = 0;
    if (!f.read(reinterpret_cast<char*>(&count), sizeof(count))) return {version, cache};
    if (count > 100000) return {version, cache}; // sanity cap

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

    return {version, cache};
}

void migrate_hash_cache(SongCache& hash_cache, const fs::path& cache_path) {
    spdlog::info("hash_cache: migrating {} entries to version {}", hash_cache.size(), CACHE_VERSION);
    std::unordered_map<std::string, std::string> old_to_new;

    for (auto& [path_str, entry] : hash_cache) {
        try {
            fs::path p(path_str);
            SongParser parser(p);
            std::array<std::string, 5> new_hashes = {};
            if (p.extension() == ".osu") {
                new_hashes[0] = parser.get_diff_hash(0);
            } else {
                for (const auto& [course, course_data] : parser.metadata.course_data) {
                    if (course < 0 || course >= static_cast<int>(new_hashes.size()))
                        continue;
                    new_hashes[course] = parser.get_diff_hash(course);
                }
            }
            for (int i = 0; i < 5; i++) {
                if (!entry.hashes[i].empty() && !new_hashes[i].empty()
                        && entry.hashes[i] != new_hashes[i]) {
                    old_to_new[entry.hashes[i]] = new_hashes[i];
                }
            }
            entry.hashes = new_hashes;
            spdlog::info("hash_cache migration: updated hash for {}", path_str);
        } catch (const std::exception& e) {
            spdlog::warn("hash_cache migration: skipping {}: {}", path_str, e.what());
        }
    }

    scores_manager.remap_hashes(old_to_new);
    save_hash_cache(hash_cache, cache_path);
    spdlog::info("hash_cache: migration complete, {} hashes remapped", old_to_new.size());
}

void extract_osz(const fs::path& osz_path) {
    fs::path out_dir = osz_path.parent_path() / osz_path.stem();
    std::error_code ec;
    fs::create_directories(out_dir, ec);

    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, osz_path.string().c_str(), 0)) {
        spdlog::error("extract_osz: failed to open {}", osz_path.string());
        return;
    }

    int num_files = (int)mz_zip_reader_get_num_files(&zip);
    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
        if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;

        fs::path out_file = out_dir / stat.m_filename;
        fs::create_directories(out_file.parent_path(), ec);

        if (!mz_zip_reader_extract_to_file(&zip, i, out_file.string().c_str(), 0))
            spdlog::warn("extract_osz: failed to extract {} from {}", stat.m_filename, osz_path.string());
    }

    mz_zip_reader_end(&zip);
    fs::remove(osz_path, ec);
    spdlog::info("extract_osz: extracted {} to {}", osz_path.string(), out_dir.string());
}

std::vector<fs::path> get_song_files(std::vector<fs::path> root_path) {
    std::vector<fs::path> songs;
    for (const fs::path& path : root_path) {
        // First pass: extract any .osz archives
        try {
            std::vector<fs::path> osz_files;
            for (const auto& entry : fs::recursive_directory_iterator(
                     path, fs::directory_options::skip_permission_denied)) {
                if (entry.path().extension() == ".osz")
                    osz_files.push_back(entry.path());
            }
            for (const auto& osz : osz_files)
                extract_osz(osz);
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Error scanning for .osz files: {}", e.what());
        }

        // Second pass: collect .tja and .osu files
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(
                     path, std::filesystem::directory_options::skip_permission_denied)) {
                auto ext = entry.path().extension();
                if (ext == ".tja" || ext == ".osu") {
                    songs.push_back(entry.path());
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            spdlog::error("Error scanning song directory: {}", e.what());
        }
    }
    return songs;
}

rapidjson::Document read_json_file(fs::path file_path) {
    if (!fs::exists(file_path)) {
        throw std::runtime_error("File not found: " + file_path.string());
    }

    std::ifstream ifs(file_path);

    if (!ifs) {
        throw std::runtime_error("Failed to open file: " + file_path.string());
    }
    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document doc;
    doc.ParseStream(isw);

    if (doc.HasParseError()) {
        throw std::runtime_error("Failed to parse " + file_path.string() + ": " + std::to_string(doc.GetParseError()));
    }

    return doc;
}

std::vector<SongListEntry> read_song_list(const fs::path& path) {
    std::vector<SongListEntry> entries;
    std::ifstream file(path);
    if (!file) return entries;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, '|'))
            fields.push_back(field);
        if (!line.empty() && line.back() == '|')
            fields.push_back("");

        if (fields.size() < 3) continue;

        std::string hash = fields[0];
        if (hash.size() >= 3 &&
            (unsigned char)hash[0] == 0xEF &&
            (unsigned char)hash[1] == 0xBB &&
            (unsigned char)hash[2] == 0xBF)
            hash = hash.substr(3);

        entries.push_back({std::move(hash), std::move(fields[1]), std::move(fields[2])});
    }
    return entries;
}

void write_song_list(const fs::path& path, const std::vector<SongListEntry>& entries) {
    std::ofstream out(path, std::ios::trunc);
    for (const auto& e : entries)
        out << e.hash << "|" << e.title << "|" << e.subtitle << "\n";
}
