#include "logging.h"

#include <iostream>
#include <print>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/dup_filter_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sstream>
#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>
#if defined(YATAIDON_HAS_CPPTRACE)
#include <cpptrace/cpptrace.hpp>
#endif

#include "../../platform/platform.h"

static void log_stacktrace() {
#if defined(YATAIDON_HAS_CPPTRACE)
    try {
        std::ostringstream oss;
        cpptrace::generate_trace().print(oss, false);
        spdlog::critical("Stack trace:\n{}", oss.str());
        spdlog::default_logger()->flush();
    } catch (...) {
    }
#endif
}

void handle_exception() {
    try {
        auto exception_ptr = std::current_exception();
        if (exception_ptr) {
            std::rethrow_exception(exception_ptr);
        }
    } catch (const std::exception& e) {
        spdlog::critical("Uncaught exception: {}", e.what());
    } catch (...) {
        spdlog::critical("Uncaught exception of unknown type");
    }
    log_stacktrace();
    std::_Exit(1);
}

static void install_crash_handlers() {
    std::set_terminate(handle_exception);
    platform_install_crash_handlers();
}

static spdlog::level::level_enum parse_log_level(const std::string& log_level_str) {
    if (log_level_str == "off") {
        return spdlog::level::off;
    }
    if (log_level_str == "debug") {
        return spdlog::level::debug;
    }
    if (log_level_str == "info") {
        return spdlog::level::info;
    }
    if (log_level_str == "warning") {
        return spdlog::level::warn;
    }
    if (log_level_str == "error") {
        return spdlog::level::err;
    }
    if (log_level_str == "critical") {
        return spdlog::level::critical;
    }
    std::println(stderr, "Unknown log level '{}'; using info", log_level_str);
    return spdlog::level::info;
}

static void apply_flush_policy() {
    spdlog::flush_on(spdlog::level::critical);
    // Without a periodic flush the file trails several seconds behind
    // the game, which reads like a freeze wherever the log happens to
    // stop mid-line.
    spdlog::flush_every(std::chrono::seconds(1));
}

static void setup_fallback_logging(const std::string& log_level_str) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%^%l%$] %n: %v");
    auto logger = std::make_shared<spdlog::logger>("", console_sink);
    logger->set_level(parse_log_level(log_level_str));
    spdlog::set_default_logger(logger);
    apply_flush_policy();

    install_crash_handlers();
}

void setup_logging(const std::string& log_level_str) {
    if (log_level_str == "off") {
        auto logger = std::make_shared<spdlog::logger>("", std::make_shared<spdlog::sinks::null_sink_mt>());
        logger->set_level(spdlog::level::off);
        spdlog::set_default_logger(logger);
        spdlog::flush_every(std::chrono::seconds(0));
        install_crash_handlers();
        return;
    }
    try {
#ifndef __ANDROID__
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%^%l%$] %n: %v");
#endif

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("latest.log", true);
        file_sink->set_pattern("[%H:%M:%S.%e] [%l] %n: %v");

        auto dup_filter = std::make_shared<spdlog::sinks::dup_filter_sink_mt>(std::chrono::seconds(5));
        dup_filter->add_sink(file_sink);

#ifdef __ANDROID__
        auto android_sink = std::make_shared<spdlog::sinks::android_sink_mt>("YataiDON");
        std::vector<spdlog::sink_ptr> sinks{android_sink, dup_filter};
#else
        std::vector<spdlog::sink_ptr> sinks{console_sink, dup_filter};
#endif
        auto logger = std::make_shared<spdlog::logger>("", sinks.begin(), sinks.end());

        logger->set_level(parse_log_level(log_level_str));
        spdlog::set_default_logger(logger);
        apply_flush_policy();

        install_crash_handlers();

    } catch (const std::exception& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << " -- falling back to console-only logging"
                  << std::endl;
        setup_fallback_logging(log_level_str);
    } catch (...) {
        std::cerr << "Log initialization failed with unknown exception -- falling back to console-only logging"
                  << std::endl;
        setup_fallback_logging(log_level_str);
    }
}
