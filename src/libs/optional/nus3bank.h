#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace gen4 {

struct DecodedAudio {
    int                channels    = 0;
    int                sample_rate = 0;
    int                preview_ms  = 0;
    std::vector<float> samples;    // interleaved, -1..1
    int frame_count() const { return channels ? (int)(samples.size() / channels) : 0; }
};

bool decode_nus3bank(const fs::path& path, DecodedAudio& out);

}  // namespace gen4
