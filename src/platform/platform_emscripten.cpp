#include "platform_emscripten.h"

#include <csignal>
#include <emscripten.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include "../libs/gfx.h"
#include "platform.h"

std::filesystem::path platform_get_working_directory() {
    spdlog::info("Emscripten: using virtual FS root as working directory");
    return std::filesystem::path("/");
}

void platform_install_crash_handlers() {
    // Emscripten doesn't have traditional signal handling
    // Errors are caught by JS console/window.onerror
    auto handler = [](int) { _exit(0); };
    std::signal(SIGINT, handler);
    std::signal(SIGTERM, handler);
}

void platform_close_window() {
    close_window();
}

int emscripten_run_main_loop(void (*run_frame)()) {
    // emscripten_set_main_loop never returns
    emscripten_set_main_loop(run_frame, 0, 1);
    return 0;
}

// The wasm heap and WebGL memory are too tight to keep assets between screens.
bool platform_retains_assets() {
    return false;
}

bool platform_threaded_renderer() {
    return false;
}
