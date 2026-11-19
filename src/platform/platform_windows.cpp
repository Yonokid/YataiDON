#include "platform_windows.h"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <portaudio.h>
#include <spdlog/spdlog.h>
#include <string>

#include "platform.h"
// clang-format off
#include <windows.h>
#include <dbghelp.h> // must follow windows.h

#include "../libs/gfx.h"
#include "../libs/gfx/sdl_compat.h"
// clang-format on

namespace {

    // dbghelp is not reentrant/thread-safe; serialize entry so two threads
    // crashing concurrently don't call into it at the same time.
    std::atomic_flag g_dbghelp_lock = ATOMIC_FLAG_INIT;

    void win_write(const char* data, std::size_t len) {
        HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
        DWORD written = 0;
        if (h != nullptr && h != INVALID_HANDLE_VALUE) {
            WriteFile(h, data, static_cast<DWORD>(len), &written, nullptr);
        }
    }
    void win_write(const char* msg) {
        win_write(msg, strlen(msg));
    }

    void win_write_hex(std::uintptr_t value) {
        char buf[2 + sizeof(value) * 2];
        buf[0] = '0';
        buf[1] = 'x';
        for (std::size_t i = 0; i < sizeof(value) * 2; i++) {
            int nibble = (value >> (4 * (sizeof(value) * 2 - 1 - i))) & 0xF;
            buf[2 + i] = nibble < 10 ? char('0' + nibble) : char('a' + nibble - 10);
        }
        win_write(buf, sizeof(buf));
    }

    void log_trace_from_context(CONTEXT* ctx) {
        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();

        while (g_dbghelp_lock.test_and_set(std::memory_order_acquire)) {
        }
        struct DbghelpLockGuard {
            ~DbghelpLockGuard() { g_dbghelp_lock.clear(std::memory_order_release); }
        } dbghelp_lock_guard;

        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        const bool sym_initialized = SymInitialize(process, nullptr, TRUE);
        struct SymCleanupGuard {
            HANDLE process;
            bool active;
            ~SymCleanupGuard() {
                if (active) {
                    SymCleanup(process);
                }
            }
        } sym_cleanup_guard{process, sym_initialized};
        if (!sym_initialized) {
            win_write("SymInitialize failed; stack trace may lack symbols\n");
        }

        CONTEXT ctx_copy = *ctx;
        STACKFRAME64 sf = {};
        sf.AddrPC.Mode = AddrModeFlat;
        sf.AddrStack.Mode = AddrModeFlat;
        sf.AddrFrame.Mode = AddrModeFlat;
#if defined(_M_X64)
        sf.AddrPC.Offset = ctx_copy.Rip;
        sf.AddrStack.Offset = ctx_copy.Rsp;
        sf.AddrFrame.Offset = ctx_copy.Rbp;
        const DWORD machine_type = IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_ARM64)
        sf.AddrPC.Offset = ctx_copy.Pc;
        sf.AddrStack.Offset = ctx_copy.Sp;
        sf.AddrFrame.Offset = ctx_copy.Fp;
        const DWORD machine_type = IMAGE_FILE_MACHINE_ARM64;
#elif defined(_M_IX86)
        sf.AddrPC.Offset = ctx_copy.Eip;
        sf.AddrStack.Offset = ctx_copy.Esp;
        sf.AddrFrame.Offset = ctx_copy.Ebp;
        const DWORD machine_type = IMAGE_FILE_MACHINE_I386;
#else
#error "Unsupported architecture for Windows stack walking"
#endif

        // We can't easily use cpptrace here without pulling it in, so we just use dbghelp directly
        // This is a simplified version - in practice you might want to use cpptrace
        try {
            while (StackWalk64(machine_type, process, thread, &sf, &ctx_copy, nullptr, SymFunctionTableAccess64,
                               SymGetModuleBase64, nullptr)) {
                if (sf.AddrPC.Offset == 0) {
                    break;
                }
                win_write("  0x");
                win_write_hex(sf.AddrPC.Offset);
                win_write("\n");
            }
        } catch (...) {
            win_write("(stack walk failed)\n");
            return;
        }
    }

    LONG WINAPI crash_exception_filter(EXCEPTION_POINTERS* ep) {
        const char* exc_name = "Unknown exception";
        switch (ep->ExceptionRecord->ExceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
            exc_name = "Access violation";
            break;
        case EXCEPTION_STACK_OVERFLOW:
            exc_name = "Stack overflow";
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            exc_name = "Illegal instruction";
            break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            exc_name = "FP divide by zero";
            break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            exc_name = "Int divide by zero";
            break;
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            exc_name = "Array bounds exceeded";
            break;
        }
        win_write("Crash: ");
        win_write(exc_name);
        win_write(" (code ");
        win_write_hex(static_cast<std::uintptr_t>(ep->ExceptionRecord->ExceptionCode));
        win_write(")\n");
        log_trace_from_context(ep->ContextRecord);
        std::_Exit(1);
        return EXCEPTION_EXECUTE_HANDLER;
    }

} // namespace

std::filesystem::path win32_path_from_utf8(const std::string& utf8_str) {
    if (utf8_str.empty()) {
        return {};
    }
    UINT cp = CP_UTF8;
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8_str.c_str(), -1, nullptr, 0);
    if (n <= 0) {
        cp = CP_ACP;
        n = MultiByteToWideChar(CP_ACP, 0, utf8_str.c_str(), -1, nullptr, 0);
    }
    if (n <= 1) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(n - 1), L'\0');
    if (MultiByteToWideChar(cp, 0, utf8_str.c_str(), -1, wide.data(), n) <= 0) {
        return {};
    }
    return std::filesystem::path(wide);
}

std::filesystem::path win32_path_from_encoded(const std::string& path_str, const std::string& encoding,
                                              const std::filesystem::path& base_path) {
    int codepage = (encoding.find("utf-8") != std::string::npos) ? 65001 : 932;
    int wlen = MultiByteToWideChar(codepage, 0, path_str.c_str(), -1, nullptr, 0);
    if (wlen <= 0) {
        return base_path / path_str;
    }
    std::wstring wide(wlen, 0);
    MultiByteToWideChar(codepage, 0, path_str.c_str(), -1, wide.data(), wlen);
    return base_path / wide;
}

std::filesystem::path platform_get_working_directory() {
    wchar_t buffer[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        spdlog::error("Failed to get executable path (error {}), keeping current working directory", GetLastError());
        return {};
    }
    buffer[n] = L'\0';
    std::filesystem::path exe_path(buffer);
    return exe_path.parent_path();
}

// Plain sleeps (Sleep, std::this_thread::sleep_*) round up to the ~15.6 ms system tick, and on
// Windows 11 timeBeginPeriod no longer changes that for us; a high-resolution waitable timer wakes
// within a fraction of a millisecond.
bool win32_precise_sleep_us(long long us) {
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
    thread_local HANDLE timer =
        CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) {
        return false;
    }
    LARGE_INTEGER due;
    due.QuadPart = -us * 10; // relative, in 100 ns units
    if (!SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0)) {
        return false;
    }
    WaitForSingleObject(timer, INFINITE);
    return true;
}

void win32_lower_thread_priority() {
    // Not THREAD_MODE_BACKGROUND_BEGIN: its low I/O priority made a full skin check several times slower
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
}

void platform_install_crash_handlers() {
    std::signal(SIGINT, [](int) { std::_Exit(0); });
    SetUnhandledExceptionFilter(crash_exception_filter);
}

void platform_close_window() {
    close_window();
}

bool win32_is_key_down_native(int scancode) {
    DWORD fg_pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &fg_pid);
    if (fg_pid != GetCurrentProcessId()) {
        return false;
    }

    int vk_code = 0;

    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        vk_code = 'A' + (scancode - SDL_SCANCODE_A);
    } else if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        vk_code = '1' + (scancode - SDL_SCANCODE_1);
    } else if (scancode == SDL_SCANCODE_0) {
        vk_code = '0';
    } else if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) {
        vk_code = VK_F1 + (scancode - SDL_SCANCODE_F1);
    } else if (scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_9) {
        vk_code = VK_NUMPAD1 + (scancode - SDL_SCANCODE_KP_1);
    } else {
        switch (scancode) {
        case SDL_SCANCODE_SPACE:
            vk_code = VK_SPACE;
            break;
        case SDL_SCANCODE_ESCAPE:
            vk_code = VK_ESCAPE;
            break;
        case SDL_SCANCODE_RETURN:
            vk_code = VK_RETURN;
            break;
        case SDL_SCANCODE_TAB:
            vk_code = VK_TAB;
            break;
        case SDL_SCANCODE_BACKSPACE:
            vk_code = VK_BACK;
            break;
        case SDL_SCANCODE_INSERT:
            vk_code = VK_INSERT;
            break;
        case SDL_SCANCODE_DELETE:
            vk_code = VK_DELETE;
            break;
        case SDL_SCANCODE_RIGHT:
            vk_code = VK_RIGHT;
            break;
        case SDL_SCANCODE_LEFT:
            vk_code = VK_LEFT;
            break;
        case SDL_SCANCODE_DOWN:
            vk_code = VK_DOWN;
            break;
        case SDL_SCANCODE_UP:
            vk_code = VK_UP;
            break;
        case SDL_SCANCODE_PAGEUP:
            vk_code = VK_PRIOR;
            break;
        case SDL_SCANCODE_PAGEDOWN:
            vk_code = VK_NEXT;
            break;
        case SDL_SCANCODE_HOME:
            vk_code = VK_HOME;
            break;
        case SDL_SCANCODE_END:
            vk_code = VK_END;
            break;
        case SDL_SCANCODE_CAPSLOCK:
            vk_code = VK_CAPITAL;
            break;
        case SDL_SCANCODE_SCROLLLOCK:
            vk_code = VK_SCROLL;
            break;
        case SDL_SCANCODE_NUMLOCKCLEAR:
            vk_code = VK_NUMLOCK;
            break;
        case SDL_SCANCODE_PRINTSCREEN:
            vk_code = VK_SNAPSHOT;
            break;
        case SDL_SCANCODE_PAUSE:
            vk_code = VK_PAUSE;
            break;
        case SDL_SCANCODE_LSHIFT:
            vk_code = VK_LSHIFT;
            break;
        case SDL_SCANCODE_LCTRL:
            vk_code = VK_LCONTROL;
            break;
        case SDL_SCANCODE_LALT:
            vk_code = VK_LMENU;
            break;
        case SDL_SCANCODE_LGUI:
            vk_code = VK_LWIN;
            break;
        case SDL_SCANCODE_RSHIFT:
            vk_code = VK_RSHIFT;
            break;
        case SDL_SCANCODE_RCTRL:
            vk_code = VK_RCONTROL;
            break;
        case SDL_SCANCODE_RALT:
            vk_code = VK_RMENU;
            break;
        case SDL_SCANCODE_RGUI:
            vk_code = VK_RWIN;
            break;
        case SDL_SCANCODE_APPLICATION:
            vk_code = VK_APPS;
            break;
        case SDL_SCANCODE_APOSTROPHE:
            vk_code = VK_OEM_7;
            break;
        case SDL_SCANCODE_COMMA:
            vk_code = VK_OEM_COMMA;
            break;
        case SDL_SCANCODE_MINUS:
            vk_code = VK_OEM_MINUS;
            break;
        case SDL_SCANCODE_PERIOD:
            vk_code = VK_OEM_PERIOD;
            break;
        case SDL_SCANCODE_SLASH:
            vk_code = VK_OEM_2;
            break;
        case SDL_SCANCODE_SEMICOLON:
            vk_code = VK_OEM_1;
            break;
        case SDL_SCANCODE_EQUALS:
            vk_code = VK_OEM_PLUS;
            break;
        case SDL_SCANCODE_LEFTBRACKET:
            vk_code = VK_OEM_4;
            break;
        case SDL_SCANCODE_BACKSLASH:
            vk_code = VK_OEM_5;
            break;
        case SDL_SCANCODE_RIGHTBRACKET:
            vk_code = VK_OEM_6;
            break;
        case SDL_SCANCODE_GRAVE:
            vk_code = VK_OEM_3;
            break;
        case SDL_SCANCODE_KP_0:
            vk_code = VK_NUMPAD0;
            break;
        case SDL_SCANCODE_KP_PERIOD:
            vk_code = VK_DECIMAL;
            break;
        case SDL_SCANCODE_KP_DIVIDE:
            vk_code = VK_DIVIDE;
            break;
        case SDL_SCANCODE_KP_MULTIPLY:
            vk_code = VK_MULTIPLY;
            break;
        case SDL_SCANCODE_KP_MINUS:
            vk_code = VK_SUBTRACT;
            break;
        case SDL_SCANCODE_KP_PLUS:
            vk_code = VK_ADD;
            break;
        case SDL_SCANCODE_KP_ENTER:
            vk_code = VK_RETURN;
            break; // Windows has no separate VK
        default:
            return false;
        }
    }

    return (GetAsyncKeyState(vk_code) & 0x8000) != 0;
}

std::string win32_path_to_string(const std::filesystem::path& path) {
    std::wstring wpath = path.wstring();
    int size = WideCharToMultiByte(932, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size > 0) {
        std::string result(size - 1, '\0');
        WideCharToMultiByte(932, 0, wpath.c_str(), -1, &result[0], size, nullptr, nullptr);
        return result;
    }
    return path.string(); // fallback
}

std::string win32_utf8_from_sjis(std::string_view text) {
    const int wlen = MultiByteToWideChar(932, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (wlen <= 0) {
        return std::string(text);
    }
    std::wstring wide(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(932, 0, text.data(), static_cast<int>(text.size()), wide.data(), wlen);
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wlen, nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        return std::string(text);
    }
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wlen, out.data(), len, nullptr, nullptr);
    return out;
}

bool win32_list_dir(const std::filesystem::path& dir, std::vector<Win32DirEntry>& out) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((dir.wstring() + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr,
                                FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    do {
        const wchar_t* n = fd.cFileName;
        if (n[0] == L'.' && (n[1] == L'\0' || (n[1] == L'.' && n[2] == L'\0'))) {
            continue;
        }
        Win32DirEntry e;
        e.name = n;
        e.is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        e.is_reparse = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        out.push_back(std::move(e));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return true;
}

std::wstring win32_final_path(const std::filesystem::path& path) {
    HANDLE h = CreateFileW(path.wstring().c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return {};
    }
    std::wstring result(MAX_PATH, L'\0');
    DWORD len = GetFinalPathNameByHandleW(h, result.data(), static_cast<DWORD>(result.size()), FILE_NAME_NORMALIZED);
    if (len >= result.size()) {
        result.resize(len);
        len = GetFinalPathNameByHandleW(h, result.data(), static_cast<DWORD>(result.size()), FILE_NAME_NORMALIZED);
    }
    CloseHandle(h);
    if (len == 0 || len >= result.size()) {
        return {};
    }
    result.resize(len);
    return result;
}

// PortAudio WDM-KS backend
static PaStream* g_pa_stream = nullptr;

static PaDeviceIndex find_pa_device_by_name(PaHostApiIndex host_index, const std::string& device_name) {
    const PaHostApiInfo* host_info = Pa_GetHostApiInfo(host_index);
    if (!host_info || device_name.empty()) {
        return paNoDevice;
    }
    int num_devices = Pa_GetDeviceCount();
    for (int i = 0; i < num_devices; i++) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info || !info->name) {
            continue;
        }
        if (info->hostApi == host_index && device_name == info->name) {
            return i;
        }
    }
    return paNoDevice;
}

std::vector<std::string> win32_enumerate_portaudio_devices(int host_api_type) {
    std::vector<std::string> names;
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        spdlog::warn("Failed to initialize PortAudio for enumeration: {}", Pa_GetErrorText(err));
        return names;
    }
    PaHostApiIndex host_index = Pa_HostApiTypeIdToHostApiIndex((PaHostApiTypeId)host_api_type);
    if (host_index >= 0) {
        const PaHostApiInfo* host_info = Pa_GetHostApiInfo(host_index);
        if (host_info) {
            int num_devices = Pa_GetDeviceCount();
            for (int i = 0; i < num_devices; i++) {
                const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
                if (!info || !info->name) {
                    continue;
                }
                if (info->hostApi == host_index && info->maxOutputChannels > 0) {
                    names.emplace_back(info->name);
                }
            }
        }
    }
    Pa_Terminate();
    return names;
}

int pa_stream_callback(const void* /*inputBuffer*/, void* outputBuffer, unsigned long framesPerBuffer,
                       const PaStreamCallbackTimeInfo* /*timeInfo*/, PaStreamCallbackFlags /*statusFlags*/,
                       void* userData) {
    // This will be connected to the AudioEngine::mix function
    // For now, we just silence the buffer
    float* out = static_cast<float*>(outputBuffer);
    for (unsigned long i = 0; i < framesPerBuffer * 2; ++i) {
        out[i] = 0.0f;
    }
    return paContinue;
}

bool win32_init_portaudio_wdmks(double target_sample_rate, unsigned long buffer_size, const std::string& device_name) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        spdlog::error("Failed to initialize PortAudio: {}", Pa_GetErrorText(err));
        return false;
    }

    PaHostApiIndex host_index = Pa_HostApiTypeIdToHostApiIndex(paWDMKS);
    if (host_index < 0) {
        spdlog::error("PortAudio WDM-KS host API not available");
        Pa_Terminate();
        return false;
    }

    const PaHostApiInfo* host_info = Pa_GetHostApiInfo(host_index);
    if (!host_info || host_info->defaultOutputDevice == paNoDevice) {
        spdlog::error("No output device found for PortAudio WDM-KS");
        Pa_Terminate();
        return false;
    }

    PaDeviceIndex device = host_info->defaultOutputDevice;
    if (!device_name.empty()) {
        PaDeviceIndex matched = find_pa_device_by_name(host_index, device_name);
        if (matched != paNoDevice) {
            device = matched;
        } else {
            spdlog::warn("Requested audio device '{}' not found; using default", device_name);
        }
    }

    PaStreamParameters out_params{};
    out_params.device = device;
    out_params.channelCount = 2;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency = Pa_GetDeviceInfo(out_params.device)->defaultLowOutputLatency;
    out_params.hostApiSpecificStreamInfo = nullptr;

    err = Pa_OpenStream(&g_pa_stream, nullptr, &out_params, target_sample_rate, buffer_size, paNoFlag,
                        &pa_stream_callback, nullptr);
    if (err != paNoError) {
        spdlog::error("Failed to open WDM-KS stream: {}", Pa_GetErrorText(err));
        Pa_Terminate();
        return false;
    }

    err = Pa_StartStream(g_pa_stream);
    if (err != paNoError) {
        spdlog::error("Failed to start WDM-KS stream: {}", Pa_GetErrorText(err));
        Pa_CloseStream(g_pa_stream);
        g_pa_stream = nullptr;
        Pa_Terminate();
        return false;
    }

    const PaDeviceInfo* dev_info = Pa_GetDeviceInfo(out_params.device);
    spdlog::info("Audio Device initialized successfully");
    spdlog::info("    > Backend:       PortAudio | WDM-KS");
    spdlog::info("    > Device:        {}", dev_info ? dev_info->name : "unknown");
    spdlog::info("    > Format:        Float32");
    spdlog::info("    > Channels:      2");
    spdlog::info("    > Sample rate:   {} Hz", target_sample_rate);
    spdlog::info("    > Buffer size:   {} frames (requested)", buffer_size);
    return true;
}

bool win32_init_portaudio_mme(double target_sample_rate, unsigned long buffer_size, const std::string& device_name) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        spdlog::error("Failed to initialize PortAudio: {}", Pa_GetErrorText(err));
        return false;
    }

    PaHostApiIndex host_index = Pa_HostApiTypeIdToHostApiIndex(paMME);
    if (host_index < 0) {
        spdlog::error("PortAudio MME host API not available");
        Pa_Terminate();
        return false;
    }

    const PaHostApiInfo* host_info = Pa_GetHostApiInfo(host_index);
    if (!host_info || host_info->defaultOutputDevice == paNoDevice) {
        spdlog::error("No output device found for PortAudio MME");
        Pa_Terminate();
        return false;
    }

    PaDeviceIndex device = host_info->defaultOutputDevice;
    if (!device_name.empty()) {
        PaDeviceIndex matched = find_pa_device_by_name(host_index, device_name);
        if (matched != paNoDevice) {
            device = matched;
        } else {
            spdlog::warn("Requested audio device '{}' not found; using default", device_name);
        }
    }

    PaStreamParameters out_params{};
    out_params.device = device;
    out_params.channelCount = 2;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency = Pa_GetDeviceInfo(out_params.device)->defaultLowOutputLatency;
    out_params.hostApiSpecificStreamInfo = nullptr;

    err = Pa_OpenStream(&g_pa_stream, nullptr, &out_params, target_sample_rate, buffer_size, paNoFlag,
                        &pa_stream_callback, nullptr);
    if (err != paNoError) {
        spdlog::error("Failed to open MME stream: {}", Pa_GetErrorText(err));
        Pa_Terminate();
        return false;
    }

    err = Pa_StartStream(g_pa_stream);
    if (err != paNoError) {
        spdlog::error("Failed to start MME stream: {}", Pa_GetErrorText(err));
        Pa_CloseStream(g_pa_stream);
        g_pa_stream = nullptr;
        Pa_Terminate();
        return false;
    }

    const PaDeviceInfo* dev_info = Pa_GetDeviceInfo(out_params.device);
    spdlog::info("Audio Device initialized successfully");
    spdlog::info("    > Backend:       PortAudio | MME");
    spdlog::info("    > Device:        {}", dev_info ? dev_info->name : "unknown");
    spdlog::info("    > Format:        Float32");
    spdlog::info("    > Channels:      2");
    spdlog::info("    > Sample rate:   {} Hz", target_sample_rate);
    spdlog::info("    > Buffer size:   {} frames (requested)", buffer_size);
    return true;
}

void win32_close_portaudio() {
    if (g_pa_stream != nullptr) {
        Pa_StopStream(g_pa_stream);
        Pa_CloseStream(g_pa_stream);
        g_pa_stream = nullptr;
        Pa_Terminate();
    }
}

bool platform_retains_assets() {
    return true;
}

bool platform_threaded_renderer() {
    return false;
}
