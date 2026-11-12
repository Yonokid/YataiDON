#pragma once

#include <mutex>
#include <string>
#include <utility>
#include <vector>

// Callable from any thread; the Toast overlay drains it on the main thread.
namespace notify_detail {
    inline std::mutex mutex;
    inline std::vector<std::string> pending;
} // namespace notify_detail

inline void notify(std::string text) {
    std::lock_guard lock(notify_detail::mutex);
    notify_detail::pending.push_back(std::move(text));
}

inline std::vector<std::string> take_notifications() {
    std::lock_guard lock(notify_detail::mutex);
    return std::exchange(notify_detail::pending, {});
}
