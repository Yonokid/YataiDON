#include <climits>
#include <cstring>
#include <dlfcn.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include "../libs/gfx.h"
#include "../libs/gfx/sdl_compat.h"
#include "platform.h"

// Crash handlers live in platform_posix.cpp.

std::filesystem::path platform_get_working_directory() {
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len == -1) {
        spdlog::error("Failed to get executable path");
        return {};
    }
    buffer[len] = '\0';
    return std::filesystem::path(buffer).parent_path();
}

void platform_close_window() {
    // SDL unloads EGL before disconnecting Wayland; queued NVIDIA events still
    // reference this module's protocol metadata, so keep it loaded until then.
    void* egl_wayland = nullptr;
    const char* driver = SDL_GetCurrentVideoDriver();

    if (driver && std::strcmp(driver, "wayland") == 0) {
        egl_wayland = dlopen("libnvidia-egl-wayland.so.1", RTLD_LAZY | RTLD_NOLOAD);
    }

    close_window();

    if (egl_wayland) {
        dlclose(egl_wayland);
    }
}

bool platform_threaded_renderer() {
    return true;
}
