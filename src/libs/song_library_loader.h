#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "song_library.h"

struct TJAMetadata;

bool is_library_song_file(const std::filesystem::path& path);
SongMetadata song_metadata_from(const TJAMetadata& metadata);

class SongLibraryLoader {
public:
    explicit SongLibraryLoader(SongLibrary& library);
    ~SongLibraryLoader();

    void start(const std::vector<std::filesystem::path>& roots);
    void stop();

private:
    void load(const std::vector<std::filesystem::path>& roots);

    SongLibrary& library;
    std::mutex lifecycle_mutex;
    std::thread worker;
    std::atomic<bool> stopping{false};
};

SongLibraryLoader& get_song_library_loader();
