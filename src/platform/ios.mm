#include "ios.h"
#import <Foundation/Foundation.h>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <stdexcept>

namespace {
std::mutex clock_mutex;
bool suspended = false;
double paused_at = 0;
double paused_duration = 0;

double monotonic_ms() {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

double ios_game_time_ms() {
    std::lock_guard<std::mutex> lock(clock_mutex);
    return (suspended ? paused_at : monotonic_ms()) - paused_duration;
}

void ios_set_suspended(bool value) {
    std::lock_guard<std::mutex> lock(clock_mutex);
    if (value == suspended) return;
    if (value) paused_at = monotonic_ms();
    else paused_duration += monotonic_ms() - paused_at;
    suspended = value;
}

bool ios_is_suspended() {
    std::lock_guard<std::mutex> lock(clock_mutex);
    return suspended;
}

void ios_prepare_filesystem() {
    namespace fs = std::filesystem;
    @autoreleasepool {
        NSURL* documents = [[[NSFileManager defaultManager]
            URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] firstObject];
        if (!documents) throw std::runtime_error("Cannot locate iOS Documents directory");
        fs::path destination(documents.fileSystemRepresentation);
        fs::path resources([NSBundle mainBundle].resourcePath.fileSystemRepresentation);
        resources /= "GameData";
        fs::create_directories(destination);

        // Keep the existing relative-path asset loaders and all writable files
        // together. Copy only missing files so upgrades preserve user content.
        for (const auto& entry : fs::recursive_directory_iterator(resources)) {
            fs::path relative = fs::relative(entry.path(), resources);
            fs::path target = destination / relative;
            if (entry.is_directory()) fs::create_directories(target);
            else if (entry.is_regular_file()) {
                fs::create_directories(target.parent_path());
                // Shaders ship with the executable and must match its version.
                bool shader = *relative.begin() == "shader";
                fs::copy_file(entry.path(), target, shader ? fs::copy_options::overwrite_existing
                                                         : fs::copy_options::skip_existing);
            }
        }
        fs::create_directories(destination / "Songs");
        fs::current_path(destination);
    }
}
