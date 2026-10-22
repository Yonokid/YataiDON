#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class SkinUpdater {
public:
    struct Status {
        bool running = false;            // a pass is currently in progress
        bool finished = false;           // a pass has completed (true even when disabled)
        bool any_updated = false;        // the current pass identified >= 1 file to update
        std::string current_skin;        // skin being processed ("" between skins)
        std::string current_file;        // file being downloaded ("" while scanning)
        int files_updated = 0;           // running total of files written this pass
        int files_done = 0;              // files of current_skin downloaded (or failed) so far
        int files_total = 0;             // files of current_skin to download; 0 while checking
        std::vector<std::string> failed; // skins the last download() could not fully fetch
    };

    struct Request {
        std::string name;
        std::string url;
        std::string branch = "main";
    };

    void download(std::vector<Request> requests);

    void start();

    bool in_progress();

    bool finished();

    bool applying_updates();

    bool updated_this_pass();

    Status snapshot();

    void shutdown();

private:
    void scan_skins();
    bool update_one_skin(const std::filesystem::path& skin_dir, const std::string& repo_url, const std::string& branch);
    void set_current_skin(const std::string& name);
    void set_current_file(const std::string& file);
    void note_file_updated();
    void set_progress(int done, int total);
    void note_file_done();

    std::atomic<bool> started_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> any_updated_{false};

    std::mutex status_mutex_;
    std::string current_skin_;
    std::string current_file_;
    int files_updated_ = 0;
    int files_done_ = 0;
    int files_total_ = 0;
    std::vector<std::string> failed_;

    std::thread thread_;
};

extern SkinUpdater skin_updater;
