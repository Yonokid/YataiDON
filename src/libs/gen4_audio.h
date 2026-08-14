#pragma once

// The audio of the gen 4 arcade games: .nus3bank containers holding a BNSF
// stream tagged IS22, which is ITU-T G.719 (Siren 22) at 48 kHz.
//
// FFmpeg and libsndfile both have no G.719 decoder, so this is only built when
// YATAIDON_G719 is on, which fetches one at build time. Without it the
// functions still exist and report that the format is not supported, so
// callers need no conditional compilation of their own.

#include <cstdint>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace gen4 {

struct DecodedAudio {
    int                channels    = 0;
    int                sample_rate = 0;
    // Where the song-select preview starts, from the bank's TONE chunk, or 0
    // when the bank does not carry one.
    int                preview_ms  = 0;
    std::vector<float> samples;    // interleaved, -1..1
    int frame_count() const { return channels ? (int)(samples.size() / channels) : 0; }
};

// True when this build can decode .nus3bank audio at all.
bool audio_supported();

// Decodes the first audio stream of a .nus3bank. Returns false and logs why on
// a container it cannot read, an unsupported codec tag, or a build without
// decoder support.
bool decode_nus3bank(const fs::path& path, DecodedAudio& out);

}  // namespace gen4
