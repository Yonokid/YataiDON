#include <SDL3/SDL.h>
#include <android/native_window_jni.h>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <fstream>
#include <jni.h>
#include <mutex>
#include <unistd.h>

extern "C" {
#include <libavcodec/jni.h>
}

#include "../libs/gfx.h"
#include "../libs/input/input.h"
#include "platform.h"
#include "platform_mobile.h"

std::filesystem::path platform_get_working_directory() {
    std::filesystem::path dir("/sdcard/YataiDON");
    std::error_code ec;
    std::filesystem::create_directories(dir / "Skins", ec);
    if (!std::filesystem::exists(dir / "Skins" / ".nomedia", ec)) {
        std::ofstream(dir / "Skins" / ".nomedia");
    }
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

namespace {
    std::mutex second_mutex;
    std::condition_variable second_released;
    ANativeWindow* second_window = nullptr;
    void* second_held = nullptr;
    int second_width = 0, second_height = 0;
} // namespace

void* platform_second_screen(int& w, int& h) {
    std::scoped_lock lock(second_mutex);
    w = second_width;
    h = second_height;
    return second_window;
}

bool platform_hold_second_screen(void* window) {
    bool held = false;
    {
        std::scoped_lock lock(second_mutex);
        held = window == second_window;
        second_held = held ? window : nullptr;
    }
    second_released.notify_all();
    return held;
}

extern "C" JNIEXPORT void JNICALL Java_com_yataidon_app_YataiDONActivity_nativeDrumSurface(JNIEnv* env, jclass,
                                                                                           jobject surface, jint w,
                                                                                           jint h) {
    ANativeWindow* window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    std::unique_lock lock(second_mutex);
    ANativeWindow* old = second_window;
    second_window = window;
    second_width = w;
    second_height = h;
    if (old && old != window) {
        second_released.wait_for(lock, std::chrono::milliseconds(500), [old] { return second_held != old; });
    }
    lock.unlock();
    if (old) {
        ANativeWindow_release(old);
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_yataidon_app_YataiDONActivity_nativeDrumTouch(JNIEnv*, jclass, jboolean down,
                                                                                         jint id, jfloat x, jfloat y,
                                                                                         jint w, jint h) {
    push_second_screen_touch(down, id, x, y, w, h);
}

bool platform_threaded_renderer() {
    return false;
}

bool platform_touch_back_button() {
    return false;
}
