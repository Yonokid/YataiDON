#pragma once

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>

#include "../../platform/platform.h"

// Native directory entries have no links. Keep I/O concurrency bounded rather
// than spawning a thread for every directory in a large chart library.
template<class List>
std::optional<std::vector<PlatformFileEntry>> walk_native_directories(std::vector<PlatformFileEntry> root, List list,
                                                                      unsigned workers = 3) {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::filesystem::path> pending;
    std::vector<PlatformFileEntry> files;
    unsigned active = 0;
    bool failed = false;
    auto append = [&](std::vector<PlatformFileEntry>& entries) {
        for (auto& entry : entries) {
            if (entry.directory) {
                pending.push_back(std::move(entry.path));
            } else {
                files.push_back(std::move(entry));
            }
        }
    };
    append(root);
    auto run = [&] {
        for (;;) {
            std::filesystem::path path;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [&] { return failed || !pending.empty() || active == 0; });
                if (failed || (pending.empty() && active == 0)) {
                    return;
                }
                path = std::move(pending.front());
                pending.pop_front();
                ++active;
            }
            std::vector<PlatformFileEntry> entries;
            bool ok = false;
            try {
                ok = list(path, entries);
            } catch (...) {
                ok = false;
            }
            {
                std::scoped_lock lock(mutex);
                failed |= !ok;
                if (ok && !failed) {
                    try {
                        append(entries);
                    } catch (...) {
                        failed = true;
                    }
                }
                --active;
            }
            wake.notify_all();
        }
    };
    std::vector<std::thread> threads;
    try {
        for (unsigned i = 0; i < std::max(1u, workers); ++i) {
            threads.emplace_back(run);
        }
    } catch (...) {
        {
            std::scoped_lock lock(mutex);
            failed = true;
        }
        wake.notify_all();
    }
    for (auto& thread : threads) {
        thread.join();
    }
    if (failed) {
        return std::nullopt;
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
    return files;
}
