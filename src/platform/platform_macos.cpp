#include <climits>
#include <cstdint>
#include <cstdlib>
#include <mach-o/dyld.h>
#include <spdlog/spdlog.h>

#include "../libs/gfx.h"
#include "platform.h"

// Crash handlers live in platform_posix.cpp.

std::filesystem::path platform_get_working_directory() {
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0) {
        spdlog::error("Failed to get executable path: buffer too small");
        return {};
    }
    char resolved[PATH_MAX];
    if (realpath(buffer, resolved) == nullptr) {
        spdlog::error("Failed to resolve executable path");
        return {};
    }
    return std::filesystem::path(resolved).parent_path();
}

void platform_close_window() {
    close_window();
}

bool platform_threaded_renderer() {
    return false;
}
