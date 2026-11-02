#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

// A fresh scan owns this snapshot; timestamps are queried while the directory
// entries are hot in the filesystem cache, not after walking the entire SD tree.
class SwitchChartIndex {
    struct Stamp {
        int64_t mtime;
        int64_t size;
    };
    std::mutex mutex;
    std::unordered_map<std::string, Stamp> entries;

public:
    void clear() {
        std::scoped_lock lock(mutex);
        entries.clear();
    }

    void put(std::string path, int64_t mtime, int64_t size) {
        std::scoped_lock lock(mutex);
        entries.insert_or_assign(std::move(path), Stamp{mtime, size});
    }

    bool get(const std::string& path, int64_t& mtime, int64_t& size) {
        std::scoped_lock lock(mutex);
        auto found = entries.find(path);
        if (found == entries.end()) {
            return false;
        }
        mtime = found->second.mtime;
        size = found->second.size;
        return true;
    }
};
