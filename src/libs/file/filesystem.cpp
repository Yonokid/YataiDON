#include "filesystem.h"

#include "miniz.h" // IWYU pragma: keep
#ifdef SUPPORT_FUMEN
#include "../optional/gen3.h"
#include "../optional/gen4.h"
#endif
#include <algorithm>
#include <fstream>
#include <mutex>
#include <spdlog/spdlog.h>
#include <unordered_set>

#include "../../platform/platform.h"
#include "../../platform/platform_windows.h"
#include "../asset_cache.h"
#include "../gfx.h"

void set_working_directory_to_executable() {
    const std::filesystem::path exe_dir = platform_get_working_directory();
    if (exe_dir.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::current_path(exe_dir, ec);
    if (ec) {
        spdlog::error("Failed to set working directory to {}: {}", exe_dir.string(), ec.message());
        return;
    }
    spdlog::info("Working directory set to: {}", exe_dir.string());
}

static bool resolve_safe_extract_path(const fs::path& base_dir, const fs::path& canonical_base_dir,
                                      const std::string& entry_name, fs::path& out_file) {
    out_file = (base_dir / entry_name).lexically_normal();
    const fs::path rel = out_file.lexically_relative(base_dir);
    if (rel.empty() || *rel.begin() == "..") {
        return false;
    }

    std::error_code ec;
    fs::create_directories(out_file.parent_path(), ec);

    fs::path canonical_parent = fs::canonical(out_file.parent_path(), ec);
    if (ec) {
        return false;
    }
    const fs::path parent_rel = canonical_parent.lexically_relative(canonical_base_dir);
    return !(parent_rel.empty() || *parent_rel.begin() == "..");
}

void extract_osz(const fs::path& osz_path) {
    fs::path out_dir = osz_path.parent_path() / osz_path.stem();
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    fs::path canonical_out_dir = fs::canonical(out_dir, ec);
    if (ec) {
        spdlog::error("extract_osz: failed to resolve {}", out_dir.string());
        return;
    }

    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, osz_path.string().c_str(), 0)) {
        spdlog::error("extract_osz: failed to open {}", osz_path.string());
        return;
    }

    const auto num_files = mz_zip_reader_get_num_files(&zip);
    bool all_ok = true;
    for (auto i = 0u; i < num_files; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            all_ok = false;
            continue;
        }
        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            continue;
        }

        fs::path out_file;
        if (!resolve_safe_extract_path(out_dir, canonical_out_dir, stat.m_filename, out_file)) {
            spdlog::warn("extract_osz: skipping unsafe entry {}", stat.m_filename);
            all_ok = false;
            continue;
        }

        if (!mz_zip_reader_extract_to_file(&zip, i, out_file.string().c_str(), 0)) {
            spdlog::warn("extract_osz: failed to extract {} from {}", stat.m_filename, osz_path.string());
            all_ok = false;
        }
    }

    mz_zip_reader_end(&zip);
    if (all_ok) {
        fs::remove(osz_path, ec);
    } else {
        spdlog::warn("extract_osz: keeping {} because extraction was incomplete", osz_path.string());
    }
    spdlog::info("extract_osz: extracted {} to {}", osz_path.string(), out_dir.string());
}

void ensure_skin_extracted(const std::string& skin_name) {
    fs::path skin_dir = fs::path("Skins") / skin_name;
    fs::path marker = skin_dir / ".extracted";
    std::error_code ec;
    if (fs::exists(marker, ec)) {
        return;
    }

    fs::path zip_path = fs::path("Skins") / (skin_name + ".zip");
    if (!fs::exists(zip_path, ec)) {
        return;
    }

    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, zip_path.string().c_str(), 0)) {
        spdlog::error("ensure_skin_extracted: failed to open {}", zip_path.string());
        return;
    }

    const auto num_files = mz_zip_reader_get_num_files(&zip);

    std::string common_prefix;
    bool prefix_initialized = false;
    for (auto i = 0u; i < num_files; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            continue;
        }
        std::string name = stat.m_filename;
        auto slash = name.find('/');
        if (slash == std::string::npos) {
            common_prefix.clear();
            break;
        }
        std::string top = name.substr(0, slash + 1);
        if (!prefix_initialized) {
            common_prefix = top;
            prefix_initialized = true;
        } else if (top != common_prefix) {
            common_prefix.clear();
            break;
        }
    }

    fs::create_directories(skin_dir, ec);
    fs::path canonical_skin_dir = fs::canonical(skin_dir, ec);
    if (ec) {
        spdlog::error("ensure_skin_extracted: failed to resolve {}", skin_dir.string());
        mz_zip_reader_end(&zip);
        return;
    }
    for (auto i = 0u; i < num_files; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            continue;
        }
        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            continue;
        }

        std::string name = stat.m_filename;
        if (!common_prefix.empty() && name.starts_with(common_prefix)) {
            name = name.substr(common_prefix.size());
        }
        if (name.empty()) {
            continue;
        }

        fs::path out_file;
        if (!resolve_safe_extract_path(skin_dir, canonical_skin_dir, name, out_file)) {
            spdlog::warn("ensure_skin_extracted: skipping unsafe entry {}", stat.m_filename);
            continue;
        }

        if (!mz_zip_reader_extract_to_file(&zip, i, out_file.string().c_str(), 0)) {
            spdlog::warn("ensure_skin_extracted: failed to extract {} from {}", stat.m_filename, zip_path.string());
        }
    }

    mz_zip_reader_end(&zip);
    spdlog::info("ensure_skin_extracted: extracted {} to {}", zip_path.string(), skin_dir.string());

    std::ofstream marker_file(marker, std::ios::trunc);
    if (!marker_file) {
        spdlog::warn("ensure_skin_extracted: failed to write completion marker for {}", skin_name);
    }
}

std::vector<std::string> list_available_skins() {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(fs::path("Skins"), ec)) {
        if (e.is_directory(ec) && fs::exists(e.path() / "Graphics", ec)) {
            names.push_back(e.path().filename().string());
        } else if (e.path().extension() == ".zip") {
            names.push_back(e.path().stem().string());
        }
    }
    std::ranges::sort(names);
    names.erase(std::ranges::unique(names).begin(), names.end());
    return names;
}

#ifdef _WIN32
// Windows walk for collect_charts_from. recursive_directory_iterator stats every entry and
// fs::canonical stats every component of every folder, which took seconds on a library of a few
// thousand folders; a directory listing already carries each entry's attributes. Same paths in the
// same order as the portable walk below.
struct ChartWalk {
    std::vector<fs::path>& songs;
    std::vector<fs::path>* osz_out;
    // Resolved path of every folder entered: a link back to one of them is not followed again.
    // A plain folder resolves to its parent's resolved path plus its name, so only links are opened.
    std::unordered_set<std::wstring> visited_dirs;

    void add(const fs::path& p, const fs::path& ext, bool under_fumen) {
        if (ext == ".tja" || ext == ".osu") {
            songs.push_back(p);
        } else if (ext == ".osz") {
            if (osz_out) {
                osz_out->push_back(p);
            }
        } else if (ext == ".bin" && under_fumen) {
            songs.push_back(p);
        }
    }

    // `entries` is the listing of `dir`; `resolved` is dir's resolved path
    void walk(const fs::path& dir, const std::wstring& resolved, const std::vector<Win32DirEntry>& entries,
              bool under_fumen) {
        for (const Win32DirEntry& e : entries) {
            fs::path p = dir / e.name;
            if (!e.is_dir) {
                add(p, p.extension(), under_fumen);
                continue;
            }
            std::wstring child_resolved = e.is_reparse ? win32_final_path(p) : resolved + L'\\' + e.name;
            if (child_resolved.empty()) {
                // Cannot prove this is not a loop -> do not recurse into it.
                spdlog::warn("collect_charts_from: cannot canonicalize {}, skipping recursion", p.string());
                continue;
            }
            if (!visited_dirs.insert(child_resolved).second) {
                continue;
            }

            std::vector<Win32DirEntry> children;
            const bool listed = win32_list_dir(p, children);
#ifdef SUPPORT_FUMEN
            // Both data roots have a fumen folder: only then ask them
            bool has_fumen = false;
            for (const Win32DirEntry& c : children) {
                if (c.is_dir && _wcsicmp(c.name.c_str(), L"fumen") == 0) {
                    has_fumen = true;
                    break;
                }
            }
            if (has_fumen && (gen4::find_data_root(p) == p || gen3::find_data_root(p) == p)) {
                continue;
            }
#endif
            add(p, p.extension(), under_fumen);
            if (listed) {
                walk(p, child_resolved, children, under_fumen || e.name == L"fumen");
            }
        }
    }
};
#endif

static void collect_charts_from(const fs::path& path, std::vector<fs::path>& songs, std::vector<fs::path>* osz_out) {
#ifdef _WIN32
    std::vector<Win32DirEntry> entries;
    if (!win32_list_dir(path, entries)) {
        throw fs::filesystem_error("cannot open directory", path,
                                   std::make_error_code(std::errc::no_such_file_or_directory));
    }
    bool under_fumen = false;
    for (fs::path dir = path; !dir.empty() && dir != dir.parent_path(); dir = dir.parent_path()) {
        if (dir.filename() == "fumen") {
            under_fumen = true;
            break;
        }
    }
    ChartWalk w{songs, osz_out, {}};
    std::wstring root = win32_final_path(path);
    if (root.empty()) {
        root = fs::absolute(path).wstring();
    }
    w.visited_dirs.insert(root);
    w.walk(path, root, entries, under_fumen);
#else
    // A symlinked directory that points back at one of its own ancestors would
    // otherwise make the recursive iterator loop forever. Track the canonical
    // path of every directory entered so a symlink resolving to one of them
    // can be caught before we recurse into it again.
    std::unordered_set<std::string> visited_dirs;
    std::error_code canon_ec;
    {
        fs::path root_canonical = fs::canonical(path, canon_ec);
        if (!canon_ec) {
            visited_dirs.insert(root_canonical.string());
        }
    }
    std::error_code it_ec;
    auto it = fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied |
                                                         fs::directory_options::follow_directory_symlink);
    for (; it != fs::end(it); it.increment(it_ec)) {
        if (it_ec) {
            spdlog::warn("collect_charts_from: stopping scan of {} after iteration error: {}", path.string(),
                         it_ec.message());
            break;
        }
        const auto& entry = *it;

        std::error_code dir_ec;
        bool is_dir = entry.is_directory(dir_ec);
        if (is_dir) {
            fs::path entry_canonical = fs::canonical(entry.path(), canon_ec);
            if (canon_ec) {
                // Cannot prove this is not a loop -> do not recurse into it.
                spdlog::warn("collect_charts_from: cannot canonicalize {}, skipping recursion", entry.path().string());
                it.disable_recursion_pending();
                continue;
            }
            if (!visited_dirs.insert(entry_canonical.string()).second) {
                it.disable_recursion_pending();
                continue;
            }
        }

#ifdef SUPPORT_FUMEN
        if (is_dir && (gen4::find_data_root(entry.path()) == entry.path() ||
                       gen3::find_data_root(entry.path()) == entry.path())) {
            it.disable_recursion_pending();
            continue;
        }
#endif

        auto ext = entry.path().extension();
        if (ext == ".tja" || ext == ".osu") {
            songs.push_back(entry.path());
        } else if (ext == ".osz") {
            if (osz_out) {
                osz_out->push_back(entry.path());
            }
        } else if (ext == ".bin") {
            bool under_fumen = false;
            for (fs::path dir = entry.path().parent_path(); !dir.empty() && dir != dir.parent_path();
                 dir = dir.parent_path()) {
                if (dir.filename() == "fumen") {
                    under_fumen = true;
                    break;
                }
            }
            if (under_fumen) {
                songs.push_back(entry.path());
            }
        }
    }
#endif
}

std::vector<fs::path> get_song_files(const std::vector<fs::path>& root_path) {
    std::vector<fs::path> songs;
    for (const fs::path& path : root_path) {
#ifdef SUPPORT_FUMEN
        if (!gen4::find_data_root(path).empty() || !gen3::find_data_root(path).empty()) {
            continue;
        }
#endif

        std::vector<fs::path> osz_files;
        try {
            collect_charts_from(path, songs, &osz_files);
        } catch (const std::filesystem::filesystem_error& e) {
            spdlog::error("Error scanning song directory: {}", e.what());
            continue;
        }

        if (!osz_files.empty()) {
            for (const auto& osz : osz_files) {
                extract_osz(osz);
            }
            for (const auto& osz : osz_files) {
                fs::path out_dir = osz.parent_path() / osz.stem();
                std::error_code ec;
                if (!fs::exists(out_dir, ec)) {
                    continue;
                }
                try {
                    collect_charts_from(out_dir, songs, nullptr);
                } catch (const std::filesystem::filesystem_error& e) {
                    spdlog::error("Error scanning extracted archive {}: {}", out_dir.string(), e.what());
                }
            }
        }
    }
    std::unordered_set<std::string> seen;
    std::vector<fs::path> deduped;
    deduped.reserve(songs.size());
    for (auto& p : songs) {
        if (seen.insert(p.string()).second) {
            deduped.push_back(std::move(p));
        }
    }
    return deduped;
}

// Parsed skin JSON (texture.json etc.) by path, kept across screens.
static AssetCache<rapidjson::Document>& skin_json_cache() {
    static AssetCache<rapidjson::Document> cache(asset_cache_budget(16u << 20));
    return cache;
}

rapidjson::Document read_json_file(const fs::path& file_path) {
    if (file_path.extension() != ".json") {
        throw std::runtime_error("File is not a json file: " + file_path.string());
    }

    const std::string cache_key = file_path.lexically_normal().generic_string();
    const bool cacheable =
        skin_json_cache().enabled() && (cache_key.starts_with("Skins/") || cache_key.contains("/Skins/"));
    if (cacheable) {
        if (auto cached = skin_json_cache().get(cache_key)) {
            rapidjson::Document copy;
            copy.CopyFrom(*cached, copy.GetAllocator());
            return copy;
        }
    }
    const std::vector<unsigned char> bytes = load_file_bytes(file_path.string().c_str());
    if (bytes.empty()) {
        throw std::runtime_error((fs::exists(file_path) ? "Failed to open file: " : "File not found: ") +
                                 file_path.string());
    }
    rapidjson::Document doc;
    doc.Parse(reinterpret_cast<const char*>(bytes.data()), bytes.size());

    if (doc.HasParseError()) {
        throw std::runtime_error("Failed to parse " + file_path.string() + ": " + std::to_string(doc.GetParseError()));
    }
    if (cacheable) {
        auto cached = std::make_shared<rapidjson::Document>();
        cached->CopyFrom(doc, cached->GetAllocator());
        const size_t bytes = sizeof(rapidjson::Document) + cached->GetAllocator().Capacity();
        skin_json_cache().put(cache_key, std::move(cached), bytes);
    }

    return doc;
}

namespace {
    fs::path g_skin_graphics_path;
    fs::path g_parent_skin_graphics_path;
    std::vector<fs::path> g_skin_chain; // Graphics paths: the skin, its parent, the parent's parent ...
    std::mutex g_skin_path_mutex;

    fs::path skin_root(const fs::path& graphics_path) {
        return graphics_path.parent_path();
    }

    bool skin_has_parent_locked() {
        return g_skin_graphics_path != g_parent_skin_graphics_path;
    }
} // namespace

fs::path resolve_parent_graphics_path(const fs::path& graphics_path) {
    rapidjson::Document skin_config_file;
    try {
        skin_config_file = read_json_file(graphics_path / "skin_config.json");
    } catch (const std::exception& e) {
        spdlog::warn("resolve_parent_graphics_path: {}", e.what());
        return graphics_path;
    }
    if (skin_config_file.IsObject() && skin_config_file.HasMember("screen") && skin_config_file["screen"].IsObject() &&
        skin_config_file["screen"].HasMember("parent") && skin_config_file["screen"]["parent"].IsString()) {
        std::string parent = skin_config_file["screen"]["parent"].GetString();
        fs::path skins_root("Skins");
        fs::path candidate = (skins_root / parent).lexically_normal();
        fs::path rel = candidate.lexically_relative(skins_root);
        if (rel.empty() || *rel.begin() == "..") {
            spdlog::warn("resolve_parent_graphics_path: rejecting unsafe parent skin path '{}'", parent);
            return graphics_path;
        }
        ensure_skin_extracted(parent);
        return candidate / "Graphics";
    }
    return graphics_path;
}

std::vector<fs::path> resolve_skin_chain(const fs::path& graphics_path) {
    std::vector<fs::path> chain{graphics_path};
    for (int depth = 0; depth < 8; depth++) {
        fs::path parent = resolve_parent_graphics_path(chain.back());
        if (parent == chain.back()) {
            break;
        }
        if (std::ranges::contains(chain, parent)) {
            spdlog::warn("resolve_skin_chain: parent cycle at '{}'", parent.string());
            break;
        }
        chain.push_back(parent);
    }
    return chain;
}

void set_skin_graphics_path(const fs::path& graphics_path) {
    std::vector<fs::path> chain = resolve_skin_chain(graphics_path);
    std::scoped_lock lock(g_skin_path_mutex);
    g_skin_graphics_path = graphics_path;
    g_parent_skin_graphics_path = chain.size() > 1 ? chain[1] : graphics_path;
    g_skin_chain = std::move(chain);
}

std::vector<fs::path> skin_ancestor_roots() {
    std::scoped_lock lock(g_skin_path_mutex);
    std::vector<fs::path> roots;
    for (size_t i = 1; i < g_skin_chain.size(); i++) {
        roots.push_back(skin_root(g_skin_chain[i]));
    }
    return roots;
}

bool skin_has_parent() {
    std::scoped_lock lock(g_skin_path_mutex);
    return skin_has_parent_locked();
}

fs::path parent_skin_root() {
    std::scoped_lock lock(g_skin_path_mutex);
    return skin_root(g_parent_skin_graphics_path);
}

fs::path resolve_skin_path(const fs::path& relative_path) {
    fs::path child_root;
    std::vector<fs::path> chain;
    {
        std::scoped_lock lock(g_skin_path_mutex);
        child_root = skin_root(g_skin_graphics_path);
        chain = g_skin_chain;
    }
    fs::path child = child_root / relative_path;
    if (fs::exists(child)) {
        return child;
    }
    for (size_t i = 1; i < chain.size(); i++) {
        fs::path ancestor = skin_root(chain[i]) / relative_path;
        if (fs::exists(ancestor)) {
            return ancestor;
        }
    }
    return child;
}
