#pragma once

#include <cstdint>
#include <filesystem>
#include <rapidjson/istreamwrapper.h>
#include <stop_token>
#include <string>
#include <vector>

#include "rapidjson/document.h"

namespace fs = std::filesystem;

void set_working_directory_to_executable();

void extract_osz(const fs::path& osz_path);

void ensure_skin_extracted(const std::string& skin_name);

std::vector<std::string> list_available_skins();

std::vector<fs::path> get_song_files(const std::vector<fs::path>& root_path);

rapidjson::Document read_json_file(const fs::path& file_path);

struct SongListEntry {
    std::string hash;
    std::string title;
    std::string subtitle;
    bool operator==(const SongListEntry&) const = default;
};

struct SongListSnapshot {
    std::vector<SongListEntry> entries;
    uint64_t revision;
};

SongListSnapshot read_song_list_snapshot(const fs::path& path);

void normalize_song_list(const fs::path& path, SongListSnapshot expected, std::vector<SongListEntry> entries,
                         std::stop_token cancellation);

void init_song_list_writer();
uint64_t song_list_write_revision();

std::vector<SongListEntry> read_song_list(const fs::path& path);

void queue_song_list_write(const fs::path& path, const std::vector<SongListEntry>& entries);

fs::path resolve_parent_graphics_path(const fs::path& graphics_path);

// the skin's Graphics path followed by each ancestor's ("screen.parent", followed up to 8 deep)
std::vector<fs::path> resolve_skin_chain(const fs::path& graphics_path);

void set_skin_graphics_path(const fs::path& graphics_path);

// skin roots of every ancestor of the current skin, nearest first (empty without a parent)
std::vector<fs::path> skin_ancestor_roots();

bool skin_has_parent();

fs::path parent_skin_root();

fs::path resolve_skin_path(const fs::path& relative_path);
