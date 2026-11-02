#include <chrono>
#include <utility>

#include "navigation_load.h"

NavigationLoader::NavigationLoader() {
    worker = std::jthread([this](const std::stop_token& shutdown) { run(shutdown); });
}

NavigationLoader::~NavigationLoader() {
    shutdown();
}

void NavigationLoader::shutdown() {
    worker.request_stop();
    cancel();
    wake.notify_all();

    if (worker.joinable()) {
        worker.join();
    }
}

void NavigationLoader::submit(LoadRequest request) {
    {
        std::scoped_lock lock(mutex);
        active.request_stop();

        if (pending) {
            results.push_back({pending->request.id, LoadResult::Status::CANCELLED, {}, {}});
        }

        pending = Job{std::move(request), {}};
    }

    wake.notify_one();
}

void NavigationLoader::cancel() {
    std::scoped_lock lock(mutex);
    active.request_stop();

    if (pending) {
        results.push_back({pending->request.id, LoadResult::Status::CANCELLED, {}, {}});
        pending.reset();
    }
}

std::deque<LoadResult> NavigationLoader::take_results() {
    std::scoped_lock lock(mutex);
    std::deque<LoadResult> drained;
    drained.swap(results);
    return drained;
}

void NavigationLoader::publish(LoadResult result) {
    std::scoped_lock lock(mutex);

    if (result.status != LoadResult::Status::ENTRIES) {
        active = std::stop_source(std::nostopstate);
    }

    results.push_back(std::move(result));
}

void NavigationLoader::perform(const Job& job, const std::function<void(LoadResult)>& publish) {
    const auto stop = job.cancellation.get_token();
    LoadResult batch{job.request.id, LoadResult::Status::ENTRIES, {}, {}};
    LoadResult terminal{job.request.id, LoadResult::Status::SUCCESS, {}, {}};

    try {
        prepare_navigation(job.request, stop, [&](PreparedEntry entry) {
            if (stop.stop_requested()) {
                return;
            }

            batch.entries.push_back(std::move(entry));

            if (batch.entries.size() == 32) {
                publish(std::move(batch));
                batch = {job.request.id, LoadResult::Status::ENTRIES, {}, {}};
            }
        });

        if (stop.stop_requested()) {
            terminal.status = LoadResult::Status::CANCELLED;
        }
    } catch (const std::exception& e) {
        terminal.status = LoadResult::Status::FAILED;
        terminal.error = e.what();
    } catch (...) {
        terminal.status = LoadResult::Status::FAILED;
        terminal.error = "Unknown navigation loading error";
    }

    if (!batch.entries.empty()) {
        publish(std::move(batch));
    }

    publish(std::move(terminal));
}

void NavigationLoader::run(const std::stop_token& shutdown) {
    for (;;) {
        std::optional<Job> job;

        {
            std::unique_lock lock(mutex);
            wake.wait(lock, shutdown, [this] { return pending.has_value(); });

            if (shutdown.stop_requested()) {
                return;
            }

            if (pending->request.requires_index && !pending->request.index) {
                pending->request.index = get_song_library().snapshot();

                if (!pending->request.index && get_song_library().is_loading()) {
                    wake.wait_for(lock, std::chrono::milliseconds(4));
                    continue;
                }
            }

            job = std::move(pending);
            pending.reset();
            active = job->cancellation;
        }

        perform(*job, [this](LoadResult result) { publish(std::move(result)); });
    }
}

std::deque<LoadResult> NavigationLoader::execute(const LoadRequest& request) {
    std::deque<LoadResult> results;
    LoadRequest ready = request;

    if (ready.requires_index && !ready.index) {
        get_song_library().wait(std::atomic<bool>{false});
        ready.index = get_song_library().snapshot();
    }

    perform(Job{std::move(ready), {}}, [&](LoadResult result) { results.push_back(std::move(result)); });
    return results;
}

void NavigationResults::reset(uint64_t id) {
    request_id = id;
    pending.clear();
}

void NavigationResults::append(std::deque<LoadResult> results) {
    for (auto& result : results) {
        if (result.id == request_id) {
            pending.push_back(std::move(result));
        }
    }
}

std::optional<LoadResult> NavigationResults::apply(size_t budget, uint64_t score_revision,
                                                   const std::function<void(PreparedEntry)>& apply_entry) {
    while (!pending.empty()) {
        auto& result = pending.front();

        if (result.status != LoadResult::Status::ENTRIES) {
            auto terminal = std::move(result);
            pending.pop_front();
            return terminal;
        }

        while (!result.entries.empty() && budget > 0) {
            auto entry = std::move(result.entries.front());
            result.entries.erase(result.entries.begin());
            const auto* summary = std::get_if<PreparedFolderSummary>(&entry);

            const auto* failure = std::get_if<PreparedFolderSummaryFailure>(&entry);

            if ((!summary || summary->score_revision == score_revision) &&
                (!failure || failure->score_revision == score_revision)) {
                apply_entry(std::move(entry));
            }

            --budget;
        }

        if (!result.entries.empty()) {
            return std::nullopt;
        }

        pending.pop_front();
    }

    return std::nullopt;
}
