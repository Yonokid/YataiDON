#include "logging.h"

#include <iostream>
#include <sstream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/dup_filter_sink.h>
#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif
#include <csignal>
#include <exception>
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include <cpptrace/cpptrace.hpp>
#endif

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>

static void log_trace_from_context(CONTEXT* ctx) {
    HANDLE process = GetCurrentProcess();
    HANDLE thread  = GetCurrentThread();

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(process, nullptr, TRUE);

    CONTEXT ctx_copy = *ctx;
    STACKFRAME64 sf  = {};
    sf.AddrPC.Mode      = AddrModeFlat;
    sf.AddrPC.Offset    = ctx_copy.Rip;
    sf.AddrStack.Mode   = AddrModeFlat;
    sf.AddrStack.Offset = ctx_copy.Rsp;
    sf.AddrFrame.Mode   = AddrModeFlat;
    sf.AddrFrame.Offset = ctx_copy.Rbp;

    cpptrace::raw_trace raw;
    while (StackWalk64(
        IMAGE_FILE_MACHINE_AMD64, process, thread, &sf, &ctx_copy,
        nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr
    )) {
        if (sf.AddrPC.Offset == 0) break;
        raw.frames.push_back(static_cast<cpptrace::frame_ptr>(sf.AddrPC.Offset));
    }

    try {
        auto resolved = raw.resolve();
        std::ostringstream oss;
        resolved.print(oss, false);
        spdlog::critical("Stack trace:\n{}", oss.str());
    } catch (...) {
        spdlog::critical("(stack trace resolution failed)");
    }
    spdlog::default_logger()->flush();
}

static LONG WINAPI crash_exception_filter(EXCEPTION_POINTERS* ep) {
    const char* exc_name = "Unknown exception";
    switch (ep->ExceptionRecord->ExceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:    exc_name = "Access violation"; break;
        case EXCEPTION_STACK_OVERFLOW:      exc_name = "Stack overflow"; break;
        case EXCEPTION_ILLEGAL_INSTRUCTION: exc_name = "Illegal instruction"; break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:  exc_name = "FP divide by zero"; break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:  exc_name = "Int divide by zero"; break;
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: exc_name = "Array bounds exceeded"; break;
    }
    spdlog::critical("Crash: {} (code 0x{:08X})", exc_name,
                     static_cast<unsigned>(ep->ExceptionRecord->ExceptionCode));
    log_trace_from_context(ep->ContextRecord);
    std::_Exit(1);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

static void log_stacktrace() {
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    try {
        std::ostringstream oss;
        cpptrace::generate_trace().print(oss, false);
        spdlog::critical("Stack trace:\n{}", oss.str());
        spdlog::default_logger()->flush();
    } catch (...) {}
#endif
}

void handle_exception() {
    try {
        auto exception_ptr = std::current_exception();
        if (exception_ptr) std::rethrow_exception(exception_ptr);
    } catch (const std::exception& e) {
        spdlog::critical("Uncaught exception: {}", e.what());
    } catch (...) {
        spdlog::critical("Uncaught exception of unknown type");
    }
    log_stacktrace();
    std::_Exit(1);
}

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::exit(0);
    }
}

#ifndef _WIN32
static void crash_signal_handler(int sig) {
    const char* name = "Unknown signal";
    switch (sig) {
        case SIGSEGV: name = "SIGSEGV (Segmentation fault)"; break;
        case SIGABRT: name = "SIGABRT (Abort)"; break;
        case SIGFPE:  name = "SIGFPE (Floating point exception)"; break;
        case SIGILL:  name = "SIGILL (Illegal instruction)"; break;
    }
    spdlog::critical("Crash: {}", name);
    log_stacktrace();
    std::_Exit(1);
}
#endif

void setup_logging(const std::string& log_level_str) {
    try {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%^%l%$] %n: %v");

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            "latest.log", true);
        file_sink->set_pattern("[%l] %n: %v");

        auto dup_filter = std::make_shared<spdlog::sinks::dup_filter_sink_mt>(
            std::chrono::seconds(5));
        dup_filter->add_sink(file_sink);

#ifdef __ANDROID__
        auto android_sink = std::make_shared<spdlog::sinks::android_sink_mt>("YataiDON");
        std::vector<spdlog::sink_ptr> sinks {android_sink, dup_filter};
#else
        std::vector<spdlog::sink_ptr> sinks {console_sink, dup_filter};
#endif
        auto logger = std::make_shared<spdlog::logger>("",
            sinks.begin(), sinks.end());

        spdlog::level::level_enum level = spdlog::level::info;
        if (log_level_str == "debug") level = spdlog::level::debug;
        else if (log_level_str == "info") level = spdlog::level::info;
        else if (log_level_str == "warning") level = spdlog::level::warn;
        else if (log_level_str == "error") level = spdlog::level::err;
        else if (log_level_str == "critical") level = spdlog::level::critical;

        logger->set_level(level);
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::critical);

        std::set_terminate(handle_exception);
        std::signal(SIGINT, signal_handler);

#ifdef _WIN32
        SetUnhandledExceptionFilter(crash_exception_filter);
#else
        std::signal(SIGSEGV, crash_signal_handler);
        std::signal(SIGABRT, crash_signal_handler);
        std::signal(SIGFPE,  crash_signal_handler);
        std::signal(SIGILL,  crash_signal_handler);
#endif

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}
