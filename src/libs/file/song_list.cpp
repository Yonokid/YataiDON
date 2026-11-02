#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

#include "filesystem.h"

namespace {
    std::atomic<uint64_t> persisted_revision{0};

    using Entries = std::vector<SongListEntry>;

    fs::path song_list_destination(fs::path path) {
        path = fs::absolute(path);

        for (int depth = 0; depth < 40; ++depth) {
            path = fs::weakly_canonical(path);
            std::error_code ec;
            const auto status = fs::symlink_status(path, ec);

            if (ec && ec != std::errc::no_such_file_or_directory) {
                throw fs::filesystem_error("Cannot inspect song list", path, ec);
            }

            if (!fs::is_symlink(status)) {
                return path;
            }

            auto target = fs::read_symlink(path);
            path = target.is_absolute() ? std::move(target) : path.parent_path() / target;
        }

        throw fs::filesystem_error("Cannot resolve song list", path,
                                   std::make_error_code(std::errc::too_many_symbolic_link_levels));
    }

    std::optional<fs::path> resolve_song_list(const fs::path& path) {
        try {
            return song_list_destination(path);
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Cannot resolve song list {}: {}", path.string(), e.what());
            return std::nullopt;
        }
    }

    Entries read_file(const fs::path& path) {
        Entries entries;
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            std::vector<std::string> fields;
            std::stringstream stream(line);
            std::string field;
            while (std::getline(stream, field, '|')) {
                fields.push_back(field);
            }

            if (!line.empty() && line.back() == '|') {
                fields.emplace_back("");
            }

            if (fields.size() < 3) {
                continue;
            }

            if (fields[0].starts_with("\xEF\xBB\xBF")) {
                fields[0].erase(0, 3);
            }

            entries.push_back({std::move(fields[0]), std::move(fields[1]), std::move(fields[2])});
        }

        return entries;
    }

    void write_file(const fs::path& path, const Entries& entries) {
        std::ofstream out(path, std::ios::trunc);

        for (const auto& entry : entries) {
            out << entry.hash << '|' << entry.title << '|' << entry.subtitle << '\n';
        }

        out.flush();

        if (!out) {
            throw std::runtime_error("Failed to write song list");
        }

        out.close();

        if (!out) {
            throw std::runtime_error("Failed to close song list");
        }
    }

    class SongListWriter {
        struct State {
            uint64_t revision = 0;
            uint64_t disk_revision = 0;
            std::shared_ptr<const Entries> pending;
            std::shared_ptr<const Entries> persisting;
        };

        struct Job {
            fs::path path;
            std::shared_ptr<const Entries> entries;
            uint64_t revision;
            std::optional<Entries> expected;
            std::stop_token cancellation;
            std::shared_ptr<State> state;
        };

        std::mutex mutex;
        std::condition_variable wake;
        std::map<fs::path, std::shared_ptr<State>> states;
        std::deque<Job> jobs;
        bool stopping = false;
        std::jthread worker;

        std::shared_ptr<State> state_for(const fs::path& path) {
            {
                std::scoped_lock lock(mutex);

                if (auto found = states.find(path); found != states.end()) {
                    return found->second;
                }
            }

            std::error_code ec;
            const bool shared_file = fs::hard_link_count(path, ec) > 1 && !ec;

            for (;;) {
                std::map<fs::path, std::shared_ptr<State>> known;

                {
                    std::scoped_lock lock(mutex);

                    if (auto found = states.find(path); found != states.end()) {
                        return found->second;
                    }

                    if (!shared_file) {
                        return states.emplace(path, std::make_shared<State>()).first->second;
                    }

                    known = states;
                }

                std::shared_ptr<State> state;

                for (const auto& [other_path, other_state] : known) {
                    if (fs::equivalent(path, other_path, ec)) {
                        state = other_state;
                        break;
                    }
                }

                std::scoped_lock lock(mutex);

                if (states.size() != known.size()) {
                    continue;
                }

                if (!state) {
                    state = std::make_shared<State>();
                }

                return states.emplace(path, std::move(state)).first->second;
            }
        }

        bool current(const Job& job) {
            std::scoped_lock lock(mutex);
            return !job.cancellation.stop_requested() && job.state->revision == job.revision;
        }

        void persist(const Job& job) {
            if (!current(job)) {
                return;
            }

            const auto& destination = job.path;

            if (job.expected && read_file(destination) != *job.expected) {
                return;
            }

            if (fs::exists(destination)) {
                {
                    std::scoped_lock lock(mutex);
                    auto& state = *job.state;

                    if (job.cancellation.stop_requested() || state.revision != job.revision) {
                        return;
                    }

                    state.persisting = job.entries;
                    ++state.disk_revision;
                }

                // Keeping the inode preserves ACLs, extended attributes, and shared hard links.
                write_file(destination, *job.entries);
                ++persisted_revision;
                return;
            }

            auto temporary = destination;

            temporary +=
                ".yataidon-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp";

            try {
                write_file(temporary, *job.entries);

                if ((!job.expected || read_file(destination) == *job.expected) && current(job)) {
                    fs::rename(temporary, destination);
                    ++persisted_revision;
                }
            } catch (...) {
                std::error_code ec;
                fs::remove(temporary, ec);
                throw;
            }

            std::error_code ec;
            fs::remove(temporary, ec);
        }

        void process(const Job& job) {
            try {
                persist(job);
            } catch (const std::exception& e) {
                spdlog::error("Failed to save song list {}: {}", job.path.string(), e.what());
            } catch (...) {
                spdlog::error("Failed to save song list {}", job.path.string());
            }

            std::scoped_lock lock(mutex);
            auto& state = *job.state;

            if (state.persisting) {
                state.persisting.reset();
                ++state.disk_revision;
            }

            if (!job.expected && state.revision == job.revision) {
                state.pending.reset();
            }
        }

        void run() {
            for (;;) {
                Job job;

                {
                    std::unique_lock lock(mutex);
                    wake.wait(lock, [this] { return stopping || !jobs.empty(); });

                    if (jobs.empty()) {
                        return;
                    }

                    job = std::move(jobs.front());
                    jobs.pop_front();
                }

                process(job);
            }
        }

        void submitted() { wake.notify_one(); }

    public:
        SongListWriter() {
            // Jobs may still log while the writer drains during static destruction.
            (void)spdlog::default_logger();
            worker = std::jthread([this] { run(); });
        }

        ~SongListWriter() {
            {
                std::scoped_lock lock(mutex);
                stopping = true;
            }

            wake.notify_one();

            if (worker.joinable()) {
                worker.join();
            }
        }

        SongListSnapshot read(const fs::path& path) {
            const auto shared_state = state_for(path);

            for (;;) {
                uint64_t revision;
                uint64_t disk_revision;

                {
                    std::scoped_lock lock(mutex);
                    const auto& state = *shared_state;

                    if (state.pending) {
                        return {*state.pending, state.revision};
                    }

                    if (state.persisting) {
                        return {*state.persisting, state.revision};
                    }

                    revision = state.revision;
                    disk_revision = state.disk_revision;
                }

                auto entries = read_file(path);

                {
                    std::scoped_lock lock(mutex);
                    const auto& state = *shared_state;

                    if (state.revision == revision && state.disk_revision == disk_revision && !state.pending &&
                        !state.persisting) {
                        return {std::move(entries), revision};
                    }
                }
            }
        }

        void write(fs::path path, Entries entries) {
            auto shared_state = state_for(path);
            auto data = std::make_shared<const Entries>(std::move(entries));

            {
                std::scoped_lock lock(mutex);
                auto& state = *shared_state;
                ++state.revision;
                state.pending = data;

                jobs.push_back(
                    {std::move(path), std::move(data), state.revision, std::nullopt, {}, std::move(shared_state)});
            }

            submitted();
        }

        void normalize(fs::path path, SongListSnapshot expected, Entries entries, std::stop_token cancellation) {
            auto shared_state = state_for(path);
            auto data = std::make_shared<const Entries>(std::move(entries));

            {
                std::scoped_lock lock(mutex);

                if (cancellation.stop_requested() || shared_state->revision != expected.revision) {
                    return;
                }

                jobs.push_back({std::move(path), std::move(data), expected.revision, std::move(expected.entries),
                                std::move(cancellation), std::move(shared_state)});
            }

            submitted();
        }
    };

    SongListWriter& song_lists() {
        static SongListWriter writer;
        return writer;
    }
} // namespace

SongListSnapshot read_song_list_snapshot(const fs::path& path) {
    if (auto destination = resolve_song_list(path)) {
        return song_lists().read(*destination);
    }

    return {};
}

std::vector<SongListEntry> read_song_list(const fs::path& path) {
    return read_song_list_snapshot(path).entries;
}

void queue_song_list_write(const fs::path& path, const std::vector<SongListEntry>& entries) {
    if (auto destination = resolve_song_list(path)) {
        song_lists().write(std::move(*destination), entries);
    }
}

void normalize_song_list(const fs::path& path, SongListSnapshot expected, std::vector<SongListEntry> entries,
                         std::stop_token cancellation) {
    if (auto destination = resolve_song_list(path)) {
        song_lists().normalize(std::move(*destination), std::move(expected), std::move(entries),
                               std::move(cancellation));
    }
}

uint64_t song_list_write_revision() {
    return persisted_revision.load();
}

void init_song_list_writer() {
    song_lists();
}
