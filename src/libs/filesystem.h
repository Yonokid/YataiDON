#include "rapidjson/document.h"
#include <rapidjson/istreamwrapper.h>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

void set_working_directory_to_executable();

void extract_osz(const fs::path& osz_path);

std::vector<fs::path> get_song_files(std::vector<fs::path> root_path);

rapidjson::Document read_json_file(fs::path file_path);

struct SongListEntry {
    std::string hash;
    std::string title;
    std::string subtitle;
};

std::vector<SongListEntry> read_song_list(const fs::path& path);

void write_song_list(const fs::path& path, const std::vector<SongListEntry>& entries);
