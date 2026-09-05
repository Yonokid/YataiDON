#pragma once

#include "rapidjson/document.h"
#include <rapidjson/istreamwrapper.h>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

void set_working_directory_to_executable();

void extract_osz(const fs::path& osz_path);

void ensure_skin_extracted(const std::string& skin_name);

// Skin names found under Skins/: extracted directories (containing a
// Graphics folder) plus not-yet-extracted .zip skins (by filename stem),
// sorted and de-duplicated.
std::vector<std::string> list_available_skins();

std::vector<fs::path> get_song_files(std::vector<fs::path> root_path);

rapidjson::Document read_json_file(fs::path file_path);

struct SongListEntry {
    std::string hash;
    std::string title;
    std::string subtitle;
};

std::vector<SongListEntry> read_song_list(const fs::path& path);

void write_song_list(const fs::path& path, const std::vector<SongListEntry>& entries);

// Reads graphics_path's skin_config.json for screen.parent and returns the parent
// skin's Graphics path (Skins/<parent>/Graphics), or graphics_path unchanged if it
// has no parent.
fs::path resolve_parent_graphics_path(const fs::path& graphics_path);

// Sets the active skin (e.g. Skins/<skin>/Graphics) that skin_has_parent(),
// parent_skin_root(), and resolve_skin_path() below resolve against. Call once at
// startup; this process only ever has one skin active at a time.
void set_skin_graphics_path(const fs::path& graphics_path);

// True when the active skin (set via set_skin_graphics_path) has a parent skin.
bool skin_has_parent();

// The active skin's parent's root directory (Skins/<parent>), where its Sounds/
// Videos/Models/Scripts live alongside its Graphics. Only meaningful when
// skin_has_parent() is true.
fs::path parent_skin_root();

// relative_path is skin-root-relative, e.g. "Sounds/don.wav", "Videos/op_videos",
// resolved against the active skin (set via set_skin_graphics_path). Prefers the
// child skin's own copy; falls back to the parent's if the child doesn't have it.
// Returns the child path unchanged if neither exists (let the caller's own
// missing-file handling deal with it).
fs::path resolve_skin_path(const fs::path& relative_path);
