#include "wdmks_exclusive.h"
#ifdef _WIN32

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <setupapi.h>
#include <avrt.h>

#include <atomic>
#include <thread>
#include <vector>
#include <cstring>
#include <cstdint>
#include <spdlog/spdlog.h>

// KsCreatePin() is declared by <ks.h> and exported by ksuser.dll; mingw-w64
// ships an import lib for it (ksuser), linked in cmake/platform.cmake
// alongside setupapi.

namespace wdmks_exclusive {
namespace {

// Hardcoded rather than pulled from the DEFINE_GUIDSTRUCT-declared externs in
// ks.h/ksmedia.h (which require linking ksguid.lib) to keep this translation
// unit's link requirements minimal; these are fixed values from the KS spec.
constexpr GUID kCategoryRender = {
    0x65E8773E, 0x8F56, 0x11D0, { 0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96 }
};
constexpr GUID kPropsetPin = {
    0x8C134960, 0x51AD, 0x11CF, { 0x87, 0x8A, 0x94, 0xF8, 0x01, 0xC1, 0x00, 0x00 }
};
constexpr GUID kPropsetConnection = {
    0x1D58C920, 0xAC9B, 0x11CF, { 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00 }
};
constexpr GUID kInterfaceSetStandard = {
    0x1A8766A0, 0x62CE, 0x11CF, { 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00 }
};
constexpr GUID kMediumSetStandard = {
    0x4747B320, 0x62CE, 0x11CF, { 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00 }
};
constexpr GUID kFormatTypeAudio = {
    0x73647561, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 }
};
constexpr GUID kFormatSubtypeIeeeFloat = {
    0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 }
};
constexpr GUID kFormatSpecifierWfx = {
    0x05589F81, 0xC356, 0x11CE, { 0xBF, 0x01, 0x00, 0xAA, 0x00, 0x55, 0x59, 0x5A }
};

constexpr unsigned int kPacketCount = 3;

#pragma pack(push, 1)
struct PinConnectRequest {
    KSPIN_CONNECT connect;
    KSDATAFORMAT_WAVEFORMATEX format;
};
#pragma pack(pop)

struct RenderPacket {
    KSSTREAM_HEADER header{};
    OVERLAPPED       overlapped{};
    std::vector<uint8_t> data;
};

HANDLE               g_filter        = nullptr;
HANDLE               g_pin           = nullptr;
UINT32               g_buffer_frames = 0;
std::atomic<bool>    g_running{false};
std::thread          g_render_thread;
MixCallback          g_mix_fn = nullptr;
AudioEngine*         g_engine = nullptr;
std::vector<RenderPacket> g_packets;

bool set_pin_state(HANDLE pin, KSSTATE state) {
    KSPROPERTY prop{};
    prop.Set   = kPropsetConnection;
    prop.Id    = KSPROPERTY_CONNECTION_STATE;
    prop.Flags = KSPROPERTY_TYPE_SET;
    DWORD bytes = 0;
    return DeviceIoControl(pin, IOCTL_KS_PROPERTY, &prop, sizeof(prop),
                            &state, sizeof(state), &bytes, nullptr) != 0;
}

bool get_pin_property(HANDLE filter, ULONG pin_id, ULONG property_id, void* value, ULONG value_size) {
    KSP_PIN prop{};
    prop.Property.Set   = kPropsetPin;
    prop.Property.Id    = property_id;
    prop.Property.Flags = KSPROPERTY_TYPE_GET;
    prop.PinId   = pin_id;
    prop.Reserved = 0;
    DWORD bytes = 0;
    return DeviceIoControl(filter, IOCTL_KS_PROPERTY, &prop, sizeof(prop),
                            value, value_size, &bytes, nullptr) != 0;
}

bool get_pin_dataranges(HANDLE filter, ULONG pin_id, std::vector<uint8_t>& out) {
    KSP_PIN prop{};
    prop.Property.Set   = kPropsetPin;
    prop.Property.Id    = KSPROPERTY_PIN_DATARANGES;
    prop.Property.Flags = KSPROPERTY_TYPE_GET;
    prop.PinId   = pin_id;
    prop.Reserved = 0;

    DWORD needed = 0;
    DeviceIoControl(filter, IOCTL_KS_PROPERTY, &prop, sizeof(prop), nullptr, 0, &needed, nullptr);
    if (needed < sizeof(KSMULTIPLE_ITEM)) return false;

    out.resize(needed);
    DWORD got = 0;
    return DeviceIoControl(filter, IOCTL_KS_PROPERTY, &prop, sizeof(prop),
                            out.data(), needed, &got, nullptr) != 0;
}

// Exact match only: 32-bit float, 2ch, sample_rate. Kernel streaming bypasses
// the audio engine's format converter, so (as with WASAPI exclusive mode)
// there is no "closest match" to negotiate towards.
bool pin_accepts_format(HANDLE filter, ULONG pin_id, unsigned int sample_rate) {
    KSPIN_DATAFLOW dataflow{};
    if (!get_pin_property(filter, pin_id, KSPROPERTY_PIN_DATAFLOW, &dataflow, sizeof(dataflow)))
        return false;
    if (dataflow != KSPIN_DATAFLOW_IN) return false; // host writes into a render sink pin

    KSPIN_COMMUNICATION comm{};
    if (!get_pin_property(filter, pin_id, KSPROPERTY_PIN_COMMUNICATION, &comm, sizeof(comm)))
        return false;
    if (comm != KSPIN_COMMUNICATION_SINK && comm != KSPIN_COMMUNICATION_BOTH) return false;

    std::vector<uint8_t> ranges;
    if (!get_pin_dataranges(filter, pin_id, ranges)) return false;

    auto* item = reinterpret_cast<KSMULTIPLE_ITEM*>(ranges.data());
    uint8_t* cursor = reinterpret_cast<uint8_t*>(item + 1);
    uint8_t* end    = ranges.data() + ranges.size();

    for (ULONG i = 0; i < item->Count; ++i) {
        if (cursor + sizeof(KSDATARANGE_AUDIO) > end) break;
        auto* range = reinterpret_cast<KSDATARANGE_AUDIO*>(cursor);
        if (range->DataRange.FormatSize < sizeof(KSDATARANGE_AUDIO)) break; // malformed driver data

        if (IsEqualGUID(range->DataRange.MajorFormat, kFormatTypeAudio) &&
            IsEqualGUID(range->DataRange.SubFormat, kFormatSubtypeIeeeFloat) &&
            IsEqualGUID(range->DataRange.Specifier, kFormatSpecifierWfx) &&
            (range->MaximumChannels == static_cast<ULONG>(-1) || range->MaximumChannels >= 2) &&
            range->MinimumBitsPerSample <= 32 && range->MaximumBitsPerSample >= 32 &&
            range->MinimumSampleFrequency <= sample_rate && range->MaximumSampleFrequency >= sample_rate) {
            return true;
        }

        cursor += range->DataRange.FormatSize;
    }
    return false;
}

HANDLE create_pin(HANDLE filter, ULONG pin_id, unsigned int sample_rate) {
    PinConnectRequest req{};
    req.connect.Interface.Set   = kInterfaceSetStandard;
    req.connect.Interface.Id    = KSINTERFACE_STANDARD_STREAMING;
    req.connect.Interface.Flags = 0;
    req.connect.Medium.Set      = kMediumSetStandard;
    req.connect.Medium.Id       = KSMEDIUM_TYPE_ANYINSTANCE;
    req.connect.Medium.Flags    = 0;
    req.connect.PinId           = pin_id;
    req.connect.PinToHandle     = nullptr;
    req.connect.Priority.PriorityClass    = KSPRIORITY_NORMAL;
    req.connect.Priority.PrioritySubClass = 1;

    req.format.DataFormat.FormatSize = sizeof(KSDATAFORMAT_WAVEFORMATEX);
    req.format.DataFormat.Flags      = 0;
    req.format.DataFormat.SampleSize = 8; // 2ch * 4 bytes
    req.format.DataFormat.Reserved   = 0;
    req.format.DataFormat.MajorFormat = kFormatTypeAudio;
    req.format.DataFormat.SubFormat   = kFormatSubtypeIeeeFloat;
    req.format.DataFormat.Specifier   = kFormatSpecifierWfx;

    req.format.WaveFormatEx.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
    req.format.WaveFormatEx.nChannels       = 2;
    req.format.WaveFormatEx.nSamplesPerSec  = sample_rate;
    req.format.WaveFormatEx.wBitsPerSample  = 32;
    req.format.WaveFormatEx.nBlockAlign     = 8;
    req.format.WaveFormatEx.nAvgBytesPerSec = sample_rate * 8;
    req.format.WaveFormatEx.cbSize          = 0;

    HANDLE pin = nullptr;
    DWORD result = KsCreatePin(filter, &req.connect, GENERIC_WRITE | GENERIC_READ, &pin);
    if (result != ERROR_SUCCESS) {
        spdlog::warn("WDM-KS: KsCreatePin failed on pin {} (0x{:08x})", pin_id, result);
        return nullptr;
    }
    return pin;
}

void release_all() {
    if (g_pin) {
        set_pin_state(g_pin, KSSTATE_PAUSE);
        set_pin_state(g_pin, KSSTATE_STOP);
        CloseHandle(g_pin);
        g_pin = nullptr;
    }
    if (g_filter) { CloseHandle(g_filter); g_filter = nullptr; }
    for (auto& packet : g_packets) {
        if (packet.overlapped.hEvent) CloseHandle(packet.overlapped.hEvent);
    }
    g_packets.clear();
    g_buffer_frames = 0;
    g_mix_fn = nullptr;
    g_engine = nullptr;
}

void render_thread_proc() {
    DWORD  avrt_task_index = 0;
    HANDLE avrt_handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &avrt_task_index);

    std::vector<HANDLE> events;
    events.reserve(g_packets.size());
    for (auto& packet : g_packets) events.push_back(packet.overlapped.hEvent);

    while (g_running.load(std::memory_order_acquire)) {
        DWORD wait = WaitForMultipleObjects(static_cast<DWORD>(events.size()), events.data(), FALSE, 2000);
        if (!g_running.load(std::memory_order_acquire)) break;

        if (wait == WAIT_TIMEOUT) {
            spdlog::warn("WDM-KS: render packet wait timed out, continuing");
            continue;
        }
        if (wait < WAIT_OBJECT_0 || wait >= WAIT_OBJECT_0 + events.size()) continue;

        RenderPacket& packet = g_packets[wait - WAIT_OBJECT_0];

        DWORD bytes_transferred = 0;
        GetOverlappedResult(g_pin, &packet.overlapped, &bytes_transferred, FALSE);

        g_mix_fn(reinterpret_cast<float*>(packet.data.data()), g_buffer_frames, g_engine);

        packet.header.DataUsed   = static_cast<ULONG>(packet.data.size());
        packet.header.FrameExtent = static_cast<ULONG>(packet.data.size());
        packet.header.Data       = packet.data.data();

        DWORD bytes_returned = 0;
        BOOL ok = DeviceIoControl(g_pin, IOCTL_KS_WRITE_STREAM, nullptr, 0,
                                   &packet.header, packet.header.Size, &bytes_returned, &packet.overlapped);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            spdlog::warn("WDM-KS: packet resubmit failed (0x{:08x})", GetLastError());
        }
    }

    if (avrt_handle) AvRevertMmThreadCharacteristics(avrt_handle);
}

// Enumerates KSCATEGORY_RENDER filters and tries to instantiate a render pin
// accepting 32-bit float stereo PCM at sample_rate on each, in enumeration
// order, stopping at the first success. Raw KS has no notion of a "default"
// device (that's a mixer-level concept above KS), so this is the closest
// analogue to WASAPI's default-endpoint selection available here.
bool open_first_matching_filter(unsigned int sample_rate, HANDLE* out_filter, HANDLE* out_pin) {
    HDEVINFO dev_info = SetupDiGetClassDevs(&kCategoryRender, nullptr, nullptr,
                                             DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (dev_info == INVALID_HANDLE_VALUE) {
        spdlog::error("WDM-KS: SetupDiGetClassDevs failed (0x{:08x})", static_cast<unsigned int>(GetLastError()));
        return false;
    }

    bool found = false;
    for (DWORD device_index = 0; !found; ++device_index) {
        SP_DEVICE_INTERFACE_DATA iface_data{};
        iface_data.cbSize = sizeof(iface_data);
        if (!SetupDiEnumDeviceInterfaces(dev_info, nullptr, &kCategoryRender, device_index, &iface_data))
            break; // no more devices

        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(dev_info, &iface_data, nullptr, 0, &required, nullptr);
        if (required == 0) continue;

        std::vector<uint8_t> detail_buf(required);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detail_buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(dev_info, &iface_data, detail, required, nullptr, nullptr))
            continue;

        HANDLE filter = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
        if (filter == INVALID_HANDLE_VALUE) continue; // likely in exclusive use elsewhere; try next device

        ULONG pin_count = 0;
        if (get_pin_property(filter, 0, KSPROPERTY_PIN_CTYPES, &pin_count, sizeof(pin_count))) {
            for (ULONG pin_id = 0; pin_id < pin_count; ++pin_id) {
                if (!pin_accepts_format(filter, pin_id, sample_rate)) continue;

                HANDLE pin = create_pin(filter, pin_id, sample_rate);
                if (!pin) continue;

                *out_filter = filter;
                *out_pin    = pin;
                found = true;
                break;
            }
        }

        if (!found) CloseHandle(filter);
    }

    SetupDiDestroyDeviceInfoList(dev_info);
    return found;
}

} // namespace

bool init(unsigned int sample_rate, unsigned int buffer_size_frames, MixCallback mix_fn, AudioEngine* engine) {
    if (g_running.load()) {
        spdlog::warn("WDM-KS: init() called while already running");
        return false;
    }

    if (!open_first_matching_filter(sample_rate, &g_filter, &g_pin)) {
        spdlog::warn("WDM-KS: no render pin accepts {} Hz float stereo", sample_rate);
        release_all();
        return false;
    }

    g_buffer_frames = buffer_size_frames;

    if (!set_pin_state(g_pin, KSSTATE_ACQUIRE) || !set_pin_state(g_pin, KSSTATE_PAUSE)) {
        spdlog::error("WDM-KS: failed to bring pin to PAUSE state");
        release_all();
        return false;
    }

    g_packets.resize(kPacketCount);
    const size_t packet_bytes = static_cast<size_t>(buffer_size_frames) * 8; // 2ch * 4 bytes
    for (auto& packet : g_packets) {
        packet.data.assign(packet_bytes, 0);
        packet.overlapped.hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!packet.overlapped.hEvent) {
            spdlog::error("WDM-KS: CreateEventW failed");
            release_all();
            return false;
        }
        packet.header.Size        = sizeof(KSSTREAM_HEADER);
        packet.header.Data        = packet.data.data();
        packet.header.FrameExtent = static_cast<ULONG>(packet_bytes);
        packet.header.DataUsed    = static_cast<ULONG>(packet_bytes); // silence, primed below
    }

    g_mix_fn = mix_fn;
    g_engine = engine;

    for (auto& packet : g_packets) {
        g_mix_fn(reinterpret_cast<float*>(packet.data.data()), g_buffer_frames, g_engine);
        DWORD bytes_returned = 0;
        BOOL ok = DeviceIoControl(g_pin, IOCTL_KS_WRITE_STREAM, nullptr, 0,
                                   &packet.header, packet.header.Size, &bytes_returned, &packet.overlapped);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            spdlog::error("WDM-KS: initial packet submit failed (0x{:08x})", static_cast<unsigned int>(GetLastError()));
            release_all();
            return false;
        }
    }

    if (!set_pin_state(g_pin, KSSTATE_RUN)) {
        spdlog::error("WDM-KS: failed to bring pin to RUN state");
        release_all();
        return false;
    }

    g_running.store(true, std::memory_order_release);
    g_render_thread = std::thread(render_thread_proc);

    spdlog::info("WDM-KS: started ({} Hz, {} frames/period, {} packets)", sample_rate, g_buffer_frames, kPacketCount);
    return true;
}

void shutdown() {
    if (g_running.exchange(false)) {
        if (g_pin) CancelIoEx(g_pin, nullptr); // unblocks the render thread's wait immediately
        if (g_render_thread.joinable()) g_render_thread.join();
    }
    release_all();
}

bool is_running() {
    return g_running.load(std::memory_order_acquire);
}

} // namespace wdmks_exclusive

#endif // _WIN32
