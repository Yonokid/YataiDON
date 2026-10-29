#pragma once

#include <array>
#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct SongMetadata {
    std::string title;
    std::string subtitle;
    std::vector<std::pair<std::string, std::string>> titles;
    std::vector<std::pair<std::string, std::string>> subtitles;
    std::array<int, 5> levels = {-1, -1, -1, -1, -1};
};

class SongLibrary;

class SongIndex {
public:
    size_t song_file_count() const;
    std::optional<std::filesystem::path> find_exact_title(const std::string& title, const std::string& subtitle) const;
    std::optional<std::filesystem::path> find_song_by_title(const std::string& title,
                                                            const std::string& subtitle) const;
    std::vector<std::filesystem::path> search(const std::string& query,
                                              const std::atomic<bool>* cancel = nullptr) const;
    std::vector<std::filesystem::path> filter_difficulty(int course, int level,
                                                         const std::atomic<bool>* cancel = nullptr) const;
    std::vector<std::filesystem::path> songs_by_title(const std::atomic<bool>* cancel = nullptr) const;
    std::vector<std::filesystem::path> song_paths_under(const std::filesystem::path& root,
                                                        const std::atomic<bool>* cancel = nullptr) const;
    std::vector<std::pair<std::filesystem::path, std::array<int, 5>>>
    course_levels_in_subfolders(const std::filesystem::path& root, const std::atomic<bool>* cancel = nullptr) const;

private:
    friend class SongLibrary;
    void build_title_index();
    std::map<std::pair<std::string, std::string>, std::filesystem::path> song_files;
    std::unordered_map<std::string, std::string> song_search_text;
    std::map<std::string, std::array<int, 5>> song_levels;
    std::unordered_map<std::string, std::filesystem::path> title_subtitle_index;
    std::unordered_map<std::string, std::filesystem::path> title_only_index;
    std::vector<std::pair<std::string, std::filesystem::path>> normalized_titles;
};

class SongLibrary {
public:
    bool is_loaded() const { return load_state.load() == LoadState::READY; }

    bool is_loading() const {
        const auto state = load_state.load();
        return state == LoadState::LOADING || state == LoadState::FINALIZING;
    }

    bool begin_loading();
    void finish_loading();
    void fail_loading();
    bool wait(const std::atomic<bool>& cancel) const;
    std::shared_ptr<const SongIndex> snapshot() const;
    void add(const std::filesystem::path& path, const SongMetadata& metadata);
    size_t song_file_count() const;
    std::optional<std::filesystem::path> find_exact_title(const std::string& title, const std::string& subtitle) const;
    std::optional<std::filesystem::path> find_song_by_title(const std::string& title,
                                                            const std::string& subtitle) const;
    std::vector<std::filesystem::path> search(const std::string& query,
                                              const std::atomic<bool>* cancel = nullptr) const;
    std::vector<std::filesystem::path> filter_difficulty(int course, int level,
                                                         const std::atomic<bool>* cancel = nullptr) const;
    std::vector<std::filesystem::path> songs_by_title(const std::atomic<bool>* cancel = nullptr) const;
    std::vector<std::filesystem::path> song_paths_under(const std::filesystem::path& root,
                                                        const std::atomic<bool>* cancel = nullptr) const;
    std::vector<std::pair<std::filesystem::path, std::array<int, 5>>>
    course_levels_in_subfolders(const std::filesystem::path& root, const std::atomic<bool>* cancel = nullptr) const;

private:
    enum class LoadState {
        IDLE,
        LOADING,
        FINALIZING,
        READY,
        FAILED
    };

    std::atomic<LoadState> load_state{LoadState::IDLE};
    mutable std::mutex mutex;
    std::shared_ptr<SongIndex> building;
    std::shared_ptr<const SongIndex> published;
};

SongLibrary& get_song_library();
