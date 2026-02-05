#include "logging.h"

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
}

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::exit(0);
    }
}

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

        std::vector<spdlog::sink_ptr> sinks {console_sink, dup_filter};
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
        spdlog::flush_on(spdlog::level::err);

        std::set_terminate(handle_exception);

        std::signal(SIGINT, signal_handler);

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}
