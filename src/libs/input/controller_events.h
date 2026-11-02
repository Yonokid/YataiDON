#pragma once

namespace controller_events {
    // Shared and per-player keys represent one press, not separate queued actions.
    template<class Events>
    void consume_alias(Events& events, int key) {
        constexpr int shared_base = 10000;
        constexpr int slot_base = 50000;
        if (key > shared_base && key <= shared_base + 18) {
            for (int slot = 0; slot < 4; ++slot) {
                auto alias = events.find(slot_base + slot * 100 + key - shared_base);
                if (alias != events.end()) {
                    events.erase(alias);
                    break;
                }
            }
        } else if (key >= slot_base && key < slot_base + 400) {
            const int button = (key - slot_base) % 100;
            if (button >= 1 && button <= 18) {
                auto alias = events.find(shared_base + button);
                if (alias != events.end()) {
                    events.erase(alias);
                }
            }
        }
    }
} // namespace controller_events
