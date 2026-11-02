#include "box_song_osu.h"

#include <stdexcept>

SongBoxOsu::SongBoxOsu(const fs::path& path, const BoxDef& box_def, SongParser parser,
                       std::array<std::string, 5> prepared_hashes)
    : SongBox(path, box_def, std::move(parser), std::move(prepared_hashes)) {
    // The base constructor already parsed the metadata and owns the parser -
    // read from the member instead of keeping two more copies alive.
    text_name = this->parser.get_difficulty_name();

    is_favorite = false;
    diff_fade_in = dynamic_cast<FadeAnimation*>(tex.get_animation(12));
    if (!diff_fade_in) {
        throw std::runtime_error("SongBoxOsu: animation 12 is not a FadeAnimation");
    }
}
