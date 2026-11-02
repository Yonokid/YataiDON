#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

template<class T>
class FrameKeepAlive {
    // bgfx uniform references must survive up to two frame calls with threaded rendering.
    std::array<std::vector<std::shared_ptr<const T>>, 3> frames;
    uint64_t frame = 0;

public:
    uint64_t current_frame() const { return frame; }
    void keep(const std::shared_ptr<const T>& value) { frames[frame % frames.size()].push_back(value); }
    void advance() {
        ++frame;
        frames[frame % frames.size()].clear();
    }
    void clear() {
        for (auto& entries : frames) {
            entries.clear();
        }
        ++frame;
    }
};
