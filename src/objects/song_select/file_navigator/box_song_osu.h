#pragma once

#include "../../../libs/file/filesystem.h"
#include "box_song.h"

class SongBoxOsu : public SongBox {
public:
    SongBoxOsu(const fs::path& path, const BoxDef& box_def, SongParser parser,
               std::array<std::string, 5> prepared_hashes);
};
