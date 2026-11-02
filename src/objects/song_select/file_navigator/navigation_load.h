#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>

#include "../../../libs/file/filesystem.h"
#include "../../../libs/parsers/song_parser.h"
#include "../../../libs/song_library.h"
#include "box_base.h"

struct FolderSummary {
    std::map<int, Crown> crown;
    std::map<int, Crown> crown_p2;
    int tja_count = 0;
};

FolderSummary scan_folder(const fs::path& path, const std::string& player_1, const std::string& player_2,
                          const std::stop_token& stop);

struct PreparedPresentation {
    fs::path path;
    BoxDef box_def;
    bool preserve_order = false;
    bool is_new = false;
    std::optional<float> fade_delay;

    PreparedPresentation(fs::path path, BoxDef def) : path(std::move(path)), box_def(std::move(def)) {}
    void fade_in(float delay) { fade_delay = delay; }
};

struct PreparedSong : PreparedPresentation {
    SongParser parser;
    std::array<std::string, 5> hashes;
    std::optional<BoxDef> song_genre;
    bool is_favorite = false;

    PreparedSong(const fs::path& path, BoxDef def, SongParser parser);
};

struct PreparedFolder : PreparedPresentation {
    using PreparedPresentation::PreparedPresentation;
    std::optional<BoxDef> directory_definition;
    std::optional<bool> has_children;
    bool is_osu_folder = false;
    int tja_count = -1;
};

struct PreparedBack : PreparedPresentation {
    using PreparedPresentation::PreparedPresentation;
};

struct PreparedFolderSummary {
    fs::path path;
    FolderSummary summary;
    uint64_t score_revision;
};

struct PreparedFolderSummaryFailure {
    fs::path path;
    std::string error;
    uint64_t score_revision;
};

using PreparedEntry =
    std::variant<PreparedSong, PreparedFolder, PreparedBack, PreparedFolderSummary, PreparedFolderSummaryFailure>;

struct LoadRequest {
    enum class Kind {
        ROOTS,
        DIRECTORY,
        INLINE,
        SUMMARIES
    };

    uint64_t id = 0;
    Kind kind = Kind::DIRECTORY;
    fs::path path;
    std::vector<fs::path> roots;
    std::shared_ptr<const SongIndex> index;
    bool requires_index = false;
    BoxDef box_def{};
    BoxDef back_def{};
    std::string language;
    std::string player_1;
    std::string player_2;
    uint64_t score_revision = 0;
    std::vector<fs::path> summary_paths;
    std::string search;
    bool hide_dan = false;
    std::optional<std::array<int, 3>> difficulty;
};

struct LoadResult {
    enum class Status {
        ENTRIES,
        SUCCESS,
        CANCELLED,
        FAILED
    };

    uint64_t id;
    Status status;
    std::vector<PreparedEntry> entries;
    std::string error;
};

class NavigationResults {
public:
    void reset(uint64_t id);
    void append(std::deque<LoadResult> results);

    std::optional<LoadResult> apply(size_t budget, uint64_t score_revision,
                                    const std::function<void(PreparedEntry)>& apply_entry);

private:
    uint64_t request_id = 0;
    std::deque<LoadResult> pending;
};

BoxDef parse_navigation_box_def(const fs::path& path, const std::string& language);
BoxDef cached_navigation_box_def(std::unordered_map<std::string, BoxDef>& cache, const fs::path& path,
                                 const std::string& language);

void prepare_navigation(const LoadRequest& request, const std::stop_token& stop,
                        const std::function<void(PreparedEntry)>& emit);

class NavigationLoader {
public:
    NavigationLoader();
    ~NavigationLoader();
    void submit(LoadRequest request);
    void cancel();
    void shutdown();
    std::deque<LoadResult> take_results();
    static std::deque<LoadResult> execute(const LoadRequest& request);

private:
    struct Job {
        LoadRequest request;
        std::stop_source cancellation;
    };

    std::mutex mutex;
    std::condition_variable_any wake;
    std::optional<Job> pending;
    std::stop_source active;
    std::deque<LoadResult> results;
    std::jthread worker;

    void run(const std::stop_token& shutdown);
    void publish(LoadResult result);
    static void perform(const Job& job, const std::function<void(LoadResult)>& publish);
};
