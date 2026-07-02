#pragma once
#ifdef _WIN32

class AudioEngine;

// Raw WASAPI exclusive-mode playback, bypassing SDL3 (which only ever opens
// WASAPI in shared mode) and miniaudio (whose exclusive-mode support was
// unreliable on the drivers this project targets). Exclusive mode requires an
// exact format match with no negotiation, so init() fails outright (rather
// than falling back to a different rate/format) if the device won't accept
// the requested sample rate; the caller is expected to fall back to SDL3
// shared mode in that case.
namespace wasapi_exclusive {

using MixCallback = void(*)(float* out, unsigned int frames, AudioEngine* engine);

// Synchronous: performs device enumeration, exclusive-mode format negotiation,
// and stream setup on the calling thread, then starts a dedicated render
// thread. Returns false (with no lingering state) if exclusive mode could not
// be opened at the requested sample_rate.
bool init(unsigned int sample_rate, unsigned int buffer_size_frames,
          MixCallback mix_fn, AudioEngine* engine);

// Stops the render thread and releases all COM state. Must be called from the
// same thread that called init(). Safe to call even if init() was never
// called, or failed partway through.
void shutdown();

bool is_running();

} // namespace wasapi_exclusive

#endif // _WIN32
