#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>

class SwitchTimestampCache {
    std::mutex mutex;
    std::unordered_map<int64_t, int64_t> values;

public:
    template<class Convert>
    int64_t get(int64_t timestamp, Convert&& convert) {
        std::scoped_lock lock(mutex);
        auto found = values.find(timestamp);
        if (found != values.end()) {
            return found->second;
        }
        const int64_t result = convert(timestamp);
        values.emplace(timestamp, result);
        return result;
    }
};
