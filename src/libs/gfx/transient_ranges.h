#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

// Keep adjacent submissions in one transient page; discard every range when
// bgfx advances its frame and recycles the underlying storage.
class TransientRanges {
    uint32_t capacity = 0;
    uint32_t used = 0;

public:
    void reset() { capacity = used = 0; }

    template<class Reserve>
    std::optional<uint32_t> allocate(uint32_t count, Reserve reserve) {
        if (capacity - used < count) {
            const uint32_t available = reserve(std::max(count, 4096u));
            capacity = available >= count ? available : 0;
            used = 0;
        }
        if (capacity < count) {
            return std::nullopt;
        }
        const uint32_t first = used;
        used += count;
        return first;
    }
};
