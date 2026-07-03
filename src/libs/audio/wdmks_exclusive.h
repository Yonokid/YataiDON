#pragma once
#ifdef _WIN32

class AudioEngine;

// Raw WDM Kernel Streaming playback: talks directly to the render pin of a
// WaveCyclic/WaveRT audio filter via DeviceIoControl, bypassing WASAPI, the
// Windows audio engine/APO graph, and SDL3 (which only ever opens WASAPI)
// entirely. This is the lowest-latency, most exclusive path available on
// Windows short of a vendor ASIO driver. Like exclusive-mode WASAPI, it
// requires an exact format match with no negotiation: init() fails outright
// (rather than falling back to a different rate/format) if no render pin on
// any enumerated device accepts the requested sample rate as 32-bit float
// stereo; the caller is expected to fall back to SDL3 shared mode in that
// case.
namespace wdmks_exclusive {

using MixCallback = void(*)(float* out, unsigned int frames, AudioEngine* engine);

// Synchronous: enumerates KSCATEGORY_RENDER filters, finds the first render
// pin that accepts 32-bit float stereo PCM at sample_rate, instantiates it
// via KsCreatePin, and starts a dedicated render thread that services the
// pin's streaming I/O. Returns false (with no lingering state) if no such
// pin could be found/instantiated.
bool init(unsigned int sample_rate, unsigned int buffer_size_frames,
          MixCallback mix_fn, AudioEngine* engine);

// Stops the render thread and releases all pin/filter/device-list state.
// Must be called from the same thread that called init(). Safe to call even
// if init() was never called, or failed partway through.
void shutdown();

bool is_running();

} // namespace wdmks_exclusive

#endif // _WIN32
