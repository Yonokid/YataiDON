#include "song_library.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <thread>

#include "gfx/han_fold_table.h"

namespace fs = std::filesystem;

// Song Search normalisation: ASCII is lower-cased, full-width ASCII (U+FF01..FF5E) becomes
// half-width, and simplified / traditional Chinese characters are folded to their Japanese
// shinjitai form (han_fold_table.h), so a query in any of the three scripts matches a title
// written in another. Invalid UTF-8 bytes pass through unchanged.
static uint32_t han_fold(uint32_t cp) {
    size_t lo = 0, hi = HAN_FOLD_TABLE_SIZE;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (HAN_FOLD_TABLE[mid].from < cp) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return (lo < HAN_FOLD_TABLE_SIZE && HAN_FOLD_TABLE[lo].from == cp) ? HAN_FOLD_TABLE[lo].to : cp;
}

static std::string search_fold(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        auto b = static_cast<unsigned char>(s[i]);
        int len = b < 0x80 ? 1 : (b >> 5) == 0x6 ? 2 : (b >> 4) == 0xE ? 3 : (b >> 3) == 0x1E ? 4 : 0;
        if (len == 0 || i + len > s.size()) {
            out += static_cast<char>(b);
            i++;
            continue;
        }
        uint32_t cp = len == 1 ? b : len == 2 ? (b & 0x1F) : len == 3 ? (b & 0x0F) : (b & 0x07);
        bool ok = true;
        for (int k = 1; k < len; k++) {
            auto cb = static_cast<unsigned char>(s[i + k]);
            if ((cb & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (cb & 0x3F);
        }
        if (!ok) {
            out += static_cast<char>(b);
            i++;
            continue;
        }
        i += len;
        if (cp >= 0xFF01 && cp <= 0xFF5E) {
            cp -= 0xFEE0; // full-width ASCII
        } else if (cp == 0x3000) {
            cp = ' '; // ideographic space
        }
        if (cp < 0x80) {
            out += static_cast<char>(std::tolower(static_cast<int>(cp)));
            continue;
        }
        cp = han_fold(cp);
        if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

static void replace_all(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }

    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

static std::string normalize_title(std::string s) {
    replace_all(s, "-New Audio-", "");
    replace_all(s, "-新曲-", "");
    replace_all(s, "-Old Audio-", "");
    replace_all(s, "-旧曲-", "");

    // Keep UTF-8 (bytes >= 0x80) so a Japanese / Chinese title survives; only ASCII
    // punctuation and spaces are dropped. Stripping every non-alnum byte turned every
    // all-CJK title into "", so any two such songs compared equal.
    std::erase_if(s, [](unsigned char c) { return c < 0x80 && !std::isalnum(c); });

    std::ranges::transform(s, s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return s;
}

bool SongLibrary::begin_loading() {
    std::scoped_lock lock(mutex);
    if (is_loading() || is_loaded()) {
        return false;
    }

    building = std::make_shared<SongIndex>();
    load_state = LoadState::LOADING;
    return true;
}

void SongLibrary::finish_loading() {
    std::shared_ptr<SongIndex> index;

    {
        std::scoped_lock lock(mutex);

        if (load_state != LoadState::LOADING) {
            return;
        }

        index = building;
        load_state = LoadState::FINALIZING;
    }

    index->build_title_index();

    std::scoped_lock lock(mutex);

    if (load_state == LoadState::FINALIZING && building == index) {
        published = std::move(index);
        building.reset();
        load_state = LoadState::READY;
    }
}

void SongLibrary::fail_loading() {
    std::scoped_lock lock(mutex);

    if (is_loading()) {
        building.reset();
        load_state = LoadState::FAILED;
    }
}

bool SongLibrary::wait(const std::atomic<bool>& cancel) const {
    while (is_loading() && !cancel) {
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }

    return is_loaded() && !cancel;
}

void SongLibrary::add(const fs::path& path, const SongMetadata& metadata) {
    if (std::ranges::none_of(metadata.levels, [](int level) { return level >= 0; })) {
        return;
    }

    std::string text;
    for (const auto& [lang, title] : metadata.titles) {
        text += title + '\n';
    }

    for (const auto& [lang, subtitle] : metadata.subtitles) {
        text += subtitle + '\n';
    }

    std::scoped_lock lock(mutex);

    if (load_state != LoadState::LOADING) {
        return;
    }

    building->song_files[{metadata.title, metadata.subtitle}] = path;
    building->song_search_text[path.string()] = search_fold(text);
    building->song_levels[path.string()] = metadata.levels;
}

size_t SongIndex::song_file_count() const {
    return song_files.size();
}

std::optional<fs::path> SongIndex::find_exact_title(const std::string& title, const std::string& subtitle) const {
    auto it = song_files.find({title, subtitle});
    return it == song_files.end() ? std::nullopt : std::optional<fs::path>(it->second);
}

std::vector<fs::path> SongIndex::search(const std::string& query, const std::atomic<bool>* cancel) const {
    if (query.empty()) {
        return {};
    }

    const std::string folded = search_fold(query);
    std::vector<fs::path> hits;
    for (const auto& [key, path] : song_files) {
        if (cancel && cancel->load()) {
            break;
        }

        auto it = song_search_text.find(path.string());
        const std::string& text = it != song_search_text.end() ? it->second : search_fold(key.first);
        if (text.contains(folded)) {
            hits.push_back(path);
        }
    }

    return hits;
}

std::vector<fs::path> SongIndex::filter_difficulty(int course, int level, const std::atomic<bool>* cancel) const {
    if (course < 0 || course > 4) {
        return {};
    }

    std::vector<fs::path> hits;
    for (const auto& [path, levels] : song_levels) {
        if (cancel && cancel->load()) {
            break;
        }

        if (levels[course] == level) {
            hits.emplace_back(path);
        }
    }

    return hits;
}

std::vector<fs::path> SongIndex::songs_by_title(const std::atomic<bool>* cancel) const {
    std::vector<fs::path> paths;
    paths.reserve(song_files.size());
    for (const auto& [key, path] : song_files) {
        if (cancel && cancel->load()) {
            break;
        }

        paths.push_back(path);
    }

    return paths;
}

std::vector<fs::path> SongIndex::song_paths_under(const fs::path& root, const std::atomic<bool>* cancel) const {
    std::vector<fs::path> paths;
    for (const auto& [path_str, levels] : song_levels) {
        if (cancel && cancel->load()) {
            break;
        }

        fs::path path(path_str);
        const auto relative = path.lexically_relative(root);
        if (!relative.empty() && *relative.begin() != "..") {
            paths.push_back(std::move(path));
        }
    }

    return paths;
}

std::vector<std::pair<fs::path, std::array<int, 5>>>
SongIndex::course_levels_in_subfolders(const fs::path& root, const std::atomic<bool>* cancel) const {
    std::vector<std::pair<fs::path, std::array<int, 5>>> songs;
    for (const auto& [path_str, levels] : song_levels) {
        if (cancel && cancel->load()) {
            break;
        }

        fs::path path(path_str);
        const auto relative = path.lexically_relative(root);
        if (relative.empty() || *relative.begin() == ".." || std::distance(relative.begin(), relative.end()) < 2) {
            continue;
        }

        songs.emplace_back(std::move(path), levels);
    }

    return songs;
}

void SongIndex::build_title_index() {
    // First match in song_files order wins, as with the linear scans this replaced
    title_subtitle_index.clear();
    title_only_index.clear();
    normalized_titles.clear();
    normalized_titles.reserve(song_files.size());
    for (auto& [key, path] : song_files) {
        std::string t = normalize_title(key.first);
        title_subtitle_index.emplace(t + '\x1f' + normalize_title(key.second), path);
        title_only_index.emplace(t, path);
        normalized_titles.emplace_back(std::move(t), path);
    }
}

std::optional<fs::path> SongIndex::find_song_by_title(const std::string& title, const std::string& subtitle) const {
    std::string norm_title = normalize_title(title);
    std::string norm_subtitle = normalize_title(subtitle);
    if (norm_title.empty()) {
        return std::nullopt; // nothing to match on
    }

    if (auto it = title_subtitle_index.find(norm_title + '\x1f' + norm_subtitle); it != title_subtitle_index.end()) {
        return it->second;
    }

    // Fallback: title only
    if (auto it = title_only_index.find(norm_title); it != title_only_index.end()) {
        return it->second;
    }

    for (const auto& [indexed_title, path] : normalized_titles) {

        // Check if norm_title is inside indexed_title or vice-versa
        if (indexed_title.contains(norm_title) || norm_title.contains(indexed_title)) {

            // Safety: Don't match tiny fragments like "a" or "the"
            if (norm_title.length() < 3 && indexed_title != norm_title) {
                continue;
            }

            return path;
        }
    }

    return std::nullopt;
}

SongLibrary& get_song_library() {
    static SongLibrary library;
    return library;
}

std::shared_ptr<const SongIndex> SongLibrary::snapshot() const {
    std::scoped_lock lock(mutex);
    return published;
}

size_t SongLibrary::song_file_count() const {
    auto index = snapshot();

    if (!index) {
        return {};
    }

    return index->song_file_count();
}

std::optional<std::filesystem::path> SongLibrary::find_exact_title(const std::string& title,
                                                                   const std::string& subtitle) const {
    auto index = snapshot();

    if (!index) {
        return {};
    }

    return index->find_exact_title(title, subtitle);
}

std::optional<std::filesystem::path> SongLibrary::find_song_by_title(const std::string& title,
                                                                     const std::string& subtitle) const {
    auto index = snapshot();

    if (!index) {
        return {};
    }

    return index->find_song_by_title(title, subtitle);
}

std::vector<std::filesystem::path> SongLibrary::search(const std::string& query,
                                                       const std::atomic<bool>* cancel) const {
    auto index = snapshot();

    if (!index) {
        return {};
    }

    return index->search(query, cancel);
}

std::vector<std::filesystem::path> SongLibrary::filter_difficulty(int course, int level,
                                                                  const std::atomic<bool>* cancel) const {
    auto index = snapshot();

    if (!index) {
        return {};
    }

    return index->filter_difficulty(course, level, cancel);
}

std::vector<std::filesystem::path> SongLibrary::songs_by_title(const std::atomic<bool>* cancel) const {
    auto index = snapshot();

    if (!index) {
        return {};
    }

    return index->songs_by_title(cancel);
}

std::vector<std::filesystem::path> SongLibrary::song_paths_under(const std::filesystem::path& root,
                                                                 const std::atomic<bool>* cancel) const {
    auto index = snapshot();

    if (!index) {
        return {};
    }

    return index->song_paths_under(root, cancel);
}

std::vector<std::pair<std::filesystem::path, std::array<int, 5>>>
SongLibrary::course_levels_in_subfolders(const std::filesystem::path& root, const std::atomic<bool>* cancel) const {
    auto index = snapshot();

    if (!index) {
        return {};
    }

    return index->course_levels_in_subfolders(root, cancel);
}
