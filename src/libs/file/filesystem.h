#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
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

// Pretty-prints doc over file_path and drops the stale read_json_file cache entry.
bool write_json_file(const fs::path& file_path, const rapidjson::Document& doc);

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

void set_song_list_account(const std::string& access_code);
std::string song_list_account();
void set_song_list_folders(const fs::path& favorites, const fs::path& recent);
fs::path song_list_file(const fs::path& folder);
bool is_song_list_file(const fs::path& file);
void write_account_song_lists(const std::string& access_code, const std::vector<std::string>& favorites,
                              const std::vector<std::string>& recent);

fs::path resolve_parent_graphics_path(const fs::path& graphics_path);

// the skin's Graphics path followed by each ancestor's ("screen.parent", followed up to 8 deep)
std::vector<fs::path> resolve_skin_chain(const fs::path& graphics_path);

void set_skin_graphics_path(const fs::path& graphics_path);

// skin roots of every ancestor of the current skin, nearest first (empty without a parent)
std::vector<fs::path> skin_ancestor_roots();

bool skin_has_parent();

fs::path parent_skin_root();

fs::path resolve_skin_path(const fs::path& relative_path);

struct ScannedFolderState {
    bool has_definition;
    bool has_children;
};

// Chart and navigation metadata paths from the completed scan; nullopt outside its roots.
std::optional<std::vector<std::filesystem::path>> get_scanned_song_entries(const std::filesystem::path& directory);
std::optional<ScannedFolderState> get_scanned_folder_state(const std::filesystem::path& directory);
