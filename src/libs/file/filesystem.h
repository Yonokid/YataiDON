#pragma once

#include <filesystem>
#include <optional>
#include <rapidjson/istreamwrapper.h>
#include <string>
#include <vector>

#include "rapidjson/document.h"

namespace fs = std::filesystem;

void set_working_directory_to_executable();

void extract_osz(const fs::path& osz_path);

void ensure_skin_extracted(const std::string& skin_name);

std::vector<std::string> list_available_skins();

std::vector<fs::path> get_song_files(const std::vector<fs::path>& root_path);

// Chart and song_list.txt paths from the completed scan; nullopt outside its roots.
std::optional<std::vector<fs::path>> get_scanned_song_entries(const fs::path& directory);

rapidjson::Document read_json_file(const fs::path& file_path);

struct SongListEntry {
    std::string hash;
    std::string title;
    std::string subtitle;
};

std::vector<SongListEntry> read_song_list(const fs::path& path);

void write_song_list(const fs::path& path, const std::vector<SongListEntry>& entries);

fs::path resolve_parent_graphics_path(const fs::path& graphics_path);

// the skin's Graphics path followed by each ancestor's ("screen.parent", followed up to 8 deep)
std::vector<fs::path> resolve_skin_chain(const fs::path& graphics_path);

void set_skin_graphics_path(const fs::path& graphics_path);

// skin roots of every ancestor of the current skin, nearest first (empty without a parent)
std::vector<fs::path> skin_ancestor_roots();

bool skin_has_parent();

fs::path parent_skin_root();

fs::path resolve_skin_path(const fs::path& relative_path);
