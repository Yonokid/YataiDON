#pragma once

// The audio of the PS3 arcade games: a .nub container holding one complete
// RIFF/WAVE ATRAC3+ file, which the bundled FFmpeg decodes. The result is
// returned in the same shape as the gen 4 decoder's, so everything downstream
// treats the two eras alike.

#include "gen4_audio.h"

namespace green {

// Decodes the ATRAC3+ stream of a .nub. Returns false and logs why on a file
// it cannot read or a stream FFmpeg will not decode.
bool decode_nub(const fs::path& path, gen4::DecodedAudio& out);

}  // namespace green
