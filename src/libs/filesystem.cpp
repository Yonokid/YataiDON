#include "filesystem.h"
#include "miniz/miniz.h"
#include <fstream>
#include <spdlog/spdlog.h>
#include <unistd.h>

#ifdef _WIN32
    #include <windows.h>
#endif
#ifdef __APPLE__
    #include <mach-o/dyld.h>
#endif

void set_working_directory_to_executable() {
#ifdef __ANDROID__
    std::filesystem::path exe_dir("/sdcard/YataiDON");
    std::error_code ec;
    std::filesystem::create_directories(exe_dir, ec);
    std::filesystem::current_path(exe_dir);
    spdlog::info("Working directory set to: {}", exe_dir.string());
#elif __EMSCRIPTEN__
    spdlog::info("Emscripten: using virtual FS root as working directory");
#elif _WIN32
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::filesystem::path exe_path(buffer);
    std::filesystem::path exe_dir = exe_path.parent_path();
    std::filesystem::current_path(exe_dir);
    spdlog::info("Working directory set to: {}", exe_dir.string());
#elif __APPLE__
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0) {
        spdlog::error("Failed to get executable path: buffer too small");
        return;
    }
    char resolved[PATH_MAX];
    if (realpath(buffer, resolved) == nullptr) {
        spdlog::error("Failed to resolve executable path");
        return;
    }
    std::filesystem::path exe_dir = std::filesystem::path(resolved).parent_path();
    std::filesystem::current_path(exe_dir);
    spdlog::info("Working directory set to: {}", exe_dir.string());
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len == -1) {
        spdlog::error("Failed to get executable path");
        return;
    }
    buffer[len] = '\0';
    std::filesystem::path exe_dir = std::filesystem::path(buffer).parent_path();
    std::filesystem::current_path(exe_dir);
    spdlog::info("Working directory set to: {}", exe_dir.string());
#endif
}

void extract_osz(const fs::path& osz_path) {
    fs::path out_dir = osz_path.parent_path() / osz_path.stem();
    std::error_code ec;
    fs::create_directories(out_dir, ec);

    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, osz_path.string().c_str(), 0)) {
        spdlog::error("extract_osz: failed to open {}", osz_path.string());
        return;
    }

    int num_files = (int)mz_zip_reader_get_num_files(&zip);
    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
        if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;

        fs::path out_file = out_dir / stat.m_filename;
        fs::create_directories(out_file.parent_path(), ec);

        if (!mz_zip_reader_extract_to_file(&zip, i, out_file.string().c_str(), 0))
            spdlog::warn("extract_osz: failed to extract {} from {}", stat.m_filename, osz_path.string());
    }

    mz_zip_reader_end(&zip);
    fs::remove(osz_path, ec);
    spdlog::info("extract_osz: extracted {} to {}", osz_path.string(), out_dir.string());
}

std::vector<fs::path> get_song_files(std::vector<fs::path> root_path) {
    std::vector<fs::path> songs;
    for (const fs::path& path : root_path) {
        // First pass: extract any .osz archives
        try {
            std::vector<fs::path> osz_files;
            for (const auto& entry : fs::recursive_directory_iterator(
                     path, fs::directory_options::skip_permission_denied | fs::directory_options::follow_directory_symlink)) {
                if (entry.path().extension() == ".osz")
                    osz_files.push_back(entry.path());
            }
            for (const auto& osz : osz_files)
                extract_osz(osz);
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Error scanning for .osz files: {}", e.what());
        }

        // Second pass: collect .tja and .osu files
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(
                     path, std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink)) {
                auto ext = entry.path().extension();
                if (ext == ".tja" || ext == ".osu" || ext == ".bin") {
                    songs.push_back(entry.path());
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            spdlog::error("Error scanning song directory: {}", e.what());
        }
    }
    return songs;
}

rapidjson::Document read_json_file(fs::path file_path) {
    if (!fs::exists(file_path)) {
        throw std::runtime_error("File not found: " + file_path.string());
    }

    if (file_path.extension() != ".json") {
        throw std::runtime_error("File is not a json file: " + file_path.string());
    }

    std::ifstream ifs(file_path);

    if (!ifs) {
        throw std::runtime_error("Failed to open file: " + file_path.string());
    }
    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document doc;
    doc.ParseStream(isw);

    if (doc.HasParseError()) {
        throw std::runtime_error("Failed to parse " + file_path.string() + ": " + std::to_string(doc.GetParseError()));
    }

    return doc;
}

std::vector<SongListEntry> read_song_list(const fs::path& path) {
    std::vector<SongListEntry> entries;
    std::ifstream file(path);
    if (!file) return entries;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, '|'))
            fields.push_back(field);
        if (!line.empty() && line.back() == '|')
            fields.push_back("");

        if (fields.size() < 3) continue;

        std::string hash = fields[0];
        if (hash.size() >= 3 &&
            (unsigned char)hash[0] == 0xEF &&
            (unsigned char)hash[1] == 0xBB &&
            (unsigned char)hash[2] == 0xBF)
            hash = hash.substr(3);

        entries.push_back({std::move(hash), std::move(fields[1]), std::move(fields[2])});
    }
    return entries;
}

void write_song_list(const fs::path& path, const std::vector<SongListEntry>& entries) {
    std::ofstream out(path, std::ios::trunc);
    for (const auto& e : entries)
        out << e.hash << "|" << e.title << "|" << e.subtitle << "\n";
}
