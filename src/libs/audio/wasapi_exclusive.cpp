#include "wasapi_exclusive.h"
#ifdef _WIN32

#define INITGUID
#include <windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>

#include <atomic>
#include <thread>
#include <cstring>
#include <spdlog/spdlog.h>

namespace wasapi_exclusive {
namespace {

// Hardcoded rather than pulled from ksmedia.h to keep this translation unit's
// include set minimal; these are fixed values from the WAVEFORMATEXTENSIBLE spec.
constexpr GUID kSubFormatIeeeFloat = {
    0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 }
};
constexpr DWORD kSpeakerFrontLeft  = 0x1;
constexpr DWORD kSpeakerFrontRight = 0x2;

IMMDeviceEnumerator* g_enumerator = nullptr;
IMMDevice*           g_device     = nullptr;
IAudioClient*        g_client     = nullptr;
IAudioRenderClient*  g_render     = nullptr;
HANDLE               g_event      = nullptr;
UINT32               g_buffer_frames = 0;
bool                 g_com_owned  = false;
std::atomic<bool>    g_running{false};
std::thread          g_render_thread;
MixCallback          g_mix_fn = nullptr;
AudioEngine*         g_engine = nullptr;

void release_all() {
    if (g_render)     { g_render->Release();     g_render = nullptr; }
    if (g_client)     { g_client->Release();     g_client = nullptr; }
    if (g_device)     { g_device->Release();     g_device = nullptr; }
    if (g_enumerator) { g_enumerator->Release(); g_enumerator = nullptr; }
    if (g_event)      { CloseHandle(g_event);    g_event = nullptr; }
    if (g_com_owned)  { CoUninitialize();        g_com_owned = false; }
    g_buffer_frames = 0;
    g_mix_fn = nullptr;
    g_engine = nullptr;
}

bool format_supported_exclusive(IAudioClient* client, WAVEFORMATEX* fmt) {
    // Exclusive mode requires an exact match; passing a non-null closest-match
    // out-param is invalid, so there is no "best effort" negotiation here.
    return client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, fmt, nullptr) == S_OK;
}

void render_thread_proc() {
    DWORD  avrt_task_index = 0;
    HANDLE avrt_handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &avrt_task_index);

    while (g_running.load(std::memory_order_acquire)) {
        DWORD wait = WaitForSingleObject(g_event, 2000);
        if (wait != WAIT_OBJECT_0) {
            if (!g_running.load(std::memory_order_acquire)) break;
            spdlog::warn("WASAPI exclusive: render event wait timed out, continuing");
            continue;
        }
        if (!g_running.load(std::memory_order_acquire)) break;

        BYTE* data = nullptr;
        HRESULT hr = g_render->GetBuffer(g_buffer_frames, &data);
        if (FAILED(hr)) continue;

        g_mix_fn(reinterpret_cast<float*>(data), g_buffer_frames, g_engine);
        g_render->ReleaseBuffer(g_buffer_frames, 0);
    }

    if (avrt_handle) AvRevertMmThreadCharacteristics(avrt_handle);
}

} // namespace

bool init(unsigned int sample_rate, unsigned int buffer_size_frames, MixCallback mix_fn, AudioEngine* engine) {
    if (g_running.load()) {
        spdlog::warn("WASAPI exclusive: init() called while already running");
        return false;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (hr == S_OK || hr == S_FALSE) {
        g_com_owned = true;
    } else if (hr == RPC_E_CHANGED_MODE) {
        g_com_owned = false; // already initialized on this thread with a different concurrency model
    } else {
        spdlog::error("WASAPI exclusive: CoInitializeEx failed (0x{:08x})", static_cast<unsigned int>(hr));
        return false;
    }

    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                           IID_IMMDeviceEnumerator, reinterpret_cast<void**>(&g_enumerator));
    if (FAILED(hr)) {
        spdlog::error("WASAPI exclusive: CoCreateInstance(MMDeviceEnumerator) failed (0x{:08x})", static_cast<unsigned int>(hr));
        release_all();
        return false;
    }

    hr = g_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &g_device);
    if (FAILED(hr)) {
        spdlog::error("WASAPI exclusive: GetDefaultAudioEndpoint failed (0x{:08x})", static_cast<unsigned int>(hr));
        release_all();
        return false;
    }

    hr = g_device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&g_client));
    if (FAILED(hr)) {
        spdlog::error("WASAPI exclusive: IMMDevice::Activate failed (0x{:08x})", static_cast<unsigned int>(hr));
        release_all();
        return false;
    }

    WAVEFORMATEX plain_fmt{};
    plain_fmt.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
    plain_fmt.nChannels       = 2;
    plain_fmt.nSamplesPerSec  = sample_rate;
    plain_fmt.wBitsPerSample  = 32;
    plain_fmt.nBlockAlign     = static_cast<WORD>(plain_fmt.nChannels * plain_fmt.wBitsPerSample / 8);
    plain_fmt.nAvgBytesPerSec = plain_fmt.nSamplesPerSec * plain_fmt.nBlockAlign;
    plain_fmt.cbSize          = 0;

    WAVEFORMATEXTENSIBLE ext_fmt{};
    ext_fmt.Format                      = plain_fmt;
    ext_fmt.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
    ext_fmt.Format.cbSize               = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    ext_fmt.Samples.wValidBitsPerSample = 32;
    ext_fmt.dwChannelMask               = kSpeakerFrontLeft | kSpeakerFrontRight;
    ext_fmt.SubFormat                  = kSubFormatIeeeFloat;

    WAVEFORMATEX* chosen_fmt = nullptr;
    if (format_supported_exclusive(g_client, &plain_fmt)) {
        chosen_fmt = &plain_fmt;
    } else if (format_supported_exclusive(g_client, reinterpret_cast<WAVEFORMATEX*>(&ext_fmt))) {
        chosen_fmt = reinterpret_cast<WAVEFORMATEX*>(&ext_fmt);
    } else {
        WAVEFORMATEX* mix_fmt = nullptr;
        if (SUCCEEDED(g_client->GetMixFormat(&mix_fmt)) && mix_fmt) {
            spdlog::warn("WASAPI exclusive: {} Hz float not supported; device mix format is {} Hz, {} ch, {} bit",
                         sample_rate, mix_fmt->nSamplesPerSec, mix_fmt->nChannels, mix_fmt->wBitsPerSample);
            CoTaskMemFree(mix_fmt);
        } else {
            spdlog::warn("WASAPI exclusive: {} Hz float not supported by device", sample_rate);
        }
        release_all();
        return false;
    }

    REFERENCE_TIME hns_period = static_cast<REFERENCE_TIME>(
        10000000.0 * static_cast<double>(buffer_size_frames) / static_cast<double>(sample_rate));

    hr = g_client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                               hns_period, hns_period, chosen_fmt, nullptr);

    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        UINT32 aligned_frames = 0;
        HRESULT hr_align = g_client->GetBufferSize(&aligned_frames);
        g_client->Release();
        g_client = nullptr;

        if (FAILED(hr_align) || aligned_frames == 0) {
            spdlog::error("WASAPI exclusive: failed to recover aligned buffer size");
            release_all();
            return false;
        }

        hns_period = static_cast<REFERENCE_TIME>(
            10000000.0 * static_cast<double>(aligned_frames) / static_cast<double>(sample_rate));

        hr = g_device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&g_client));
        if (FAILED(hr)) {
            spdlog::error("WASAPI exclusive: re-Activate after alignment retry failed (0x{:08x})", static_cast<unsigned int>(hr));
            release_all();
            return false;
        }

        hr = g_client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                   hns_period, hns_period, chosen_fmt, nullptr);
    }

    if (FAILED(hr)) {
        spdlog::error("WASAPI exclusive: IAudioClient::Initialize failed (0x{:08x})", static_cast<unsigned int>(hr));
        release_all();
        return false;
    }

    hr = g_client->GetBufferSize(&g_buffer_frames);
    if (FAILED(hr) || g_buffer_frames == 0) {
        spdlog::error("WASAPI exclusive: GetBufferSize failed (0x{:08x})", static_cast<unsigned int>(hr));
        release_all();
        return false;
    }

    g_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_event) {
        spdlog::error("WASAPI exclusive: CreateEventW failed");
        release_all();
        return false;
    }

    hr = g_client->SetEventHandle(g_event);
    if (FAILED(hr)) {
        spdlog::error("WASAPI exclusive: SetEventHandle failed (0x{:08x})", static_cast<unsigned int>(hr));
        release_all();
        return false;
    }

    hr = g_client->GetService(IID_IAudioRenderClient, reinterpret_cast<void**>(&g_render));
    if (FAILED(hr)) {
        spdlog::error("WASAPI exclusive: GetService(IAudioRenderClient) failed (0x{:08x})", static_cast<unsigned int>(hr));
        release_all();
        return false;
    }

    BYTE* initial_data = nullptr;
    hr = g_render->GetBuffer(g_buffer_frames, &initial_data);
    if (FAILED(hr)) {
        spdlog::error("WASAPI exclusive: initial GetBuffer failed (0x{:08x})", static_cast<unsigned int>(hr));
        release_all();
        return false;
    }
    std::memset(initial_data, 0, static_cast<size_t>(g_buffer_frames) * chosen_fmt->nBlockAlign);
    g_render->ReleaseBuffer(g_buffer_frames, 0);

    hr = g_client->Start();
    if (FAILED(hr)) {
        spdlog::error("WASAPI exclusive: IAudioClient::Start failed (0x{:08x})", static_cast<unsigned int>(hr));
        release_all();
        return false;
    }

    g_mix_fn = mix_fn;
    g_engine = engine;
    g_running.store(true, std::memory_order_release);
    g_render_thread = std::thread(render_thread_proc);

    spdlog::info("WASAPI exclusive: started ({} Hz, {} frames/period)", sample_rate, g_buffer_frames);
    return true;
}

void shutdown() {
    if (g_running.exchange(false)) {
        if (g_event) SetEvent(g_event); // unblock the render thread's wait immediately
        if (g_render_thread.joinable()) g_render_thread.join();
        if (g_client) g_client->Stop();
    }
    release_all();
}

bool is_running() {
    return g_running.load(std::memory_order_acquire);
}

} // namespace wasapi_exclusive

#endif // _WIN32
