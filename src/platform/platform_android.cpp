#include <SDL3/SDL.h>
#include <csignal>
#include <jni.h>
#include <unistd.h>

extern "C" {
#include <libavcodec/jni.h>
}

#include "../libs/gfx.h"
#include "platform.h"
#include "platform_mobile.h"

std::filesystem::path platform_get_working_directory() {
    std::filesystem::path dir("/sdcard/YataiDON");
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void platform_install_crash_handlers() {
    // Android uses its own crash handling (tombstones, logcat)
    // We just set up basic signal handlers
    signal(SIGINT, [](int) { _exit(0); });
}

void platform_close_window() {
    close_window();
}

void platform_initialize_after_window() {
    // FFmpeg's MediaCodec decoders reach the Java side through this VM
    if (auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv())) {
        JavaVM* vm = nullptr;
        if (env->GetJavaVM(&vm) == 0) {
            av_jni_set_java_vm(vm, nullptr);
        }
    }
    mobile_install_lifecycle_handlers();
}
