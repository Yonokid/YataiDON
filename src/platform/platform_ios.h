#pragma once

// iOS-only platform services; see platform.h for the shared ones.

// Requests a low-latency preferred I/O buffer duration from AVAudioSession.
// This does not hand back or take ownership of any audio buffer - it only
// sets a latency hint on the shared AVAudioSession and returns synchronously;
// failures are logged (via spdlog::warn) and otherwise ignored.
void ios_request_audio_buffer();

// iOS-specific window configuration.
// Sets up SDL hints for iOS (orientations, audio category, etc.)
void ios_configure_window_flags(unsigned int& flags);

// iOS-specific main loop (UIKit owns the event loop).
// Returns exit code, or -1 if not on iOS.
int ios_run_main_loop(void (*run_frame)());
