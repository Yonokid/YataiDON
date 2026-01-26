
// Thank you raysan for data structures
// https://github.com/raysan5/raylib/blob/master/src/raudio.c
// This could be cleaned up significantly. I do not think
// the audio stream structure is necessary after converting the music
// stream to portaudio

#include "portaudio.h"
#include <mutex>
#ifdef _WIN32
#include "pa_asio.h"
#include <windows.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sndfile.h>
#include <string.h>
#include <samplerate.h>
#include <unistd.h>
#include <math.h>

#define LOG_INFO 0
#define LOG_WARNING 1
#define LOG_ERROR 2

static int CURRENT_LOG_LEVEL = LOG_INFO;

extern "C" {
void set_log_level(int level) {
    CURRENT_LOG_LEVEL = level;
}
} // extern "C"

#define TRACELOG(level, ...) do { \
    if (level >= CURRENT_LOG_LEVEL) { \
        const char* level_str = (level == LOG_INFO) ? "INFO" : \
                               (level == LOG_WARNING) ? "WARNING" : "ERROR"; \
        printf("[%s] AUDIO: ", level_str); \
        printf(__VA_ARGS__); \
        printf("\n"); \
        fflush(stdout); \
    } \
} while(0)

#define FREE(ptr) do { if (ptr) { free(ptr); (ptr) = NULL; } } while(0)

#define AUDIO_DEVICE_CHANNELS              2    // Device output channels: stereo

struct audio_buffer;

typedef struct wave {
    unsigned int frameCount;    // Total number of frames (considering channels)
    unsigned int sampleRate;    // Frequency (samples per second)
    unsigned int sampleSize;    // Bit depth (bits per sample): 8, 16, 32 (24 not supported)
    unsigned int channels;      // Number of channels (1-mono, 2-stereo, ...)
    void *data;                 // Buffer data pointer
} wave;

typedef struct audio_stream {
    struct audio_buffer *buffer;       // Pointer to internal data used by the audio system

    unsigned int sampleRate;    // Frequency (samples per second)
    unsigned int sampleSize;    // Bit depth (bits per sample): 8, 16, 32 (24 not supported)
    unsigned int channels;      // Number of channels (1-mono, 2-stereo, ...)
} audio_stream;

typedef struct sound {
    audio_stream stream;         // Audio stream
    unsigned int frameCount;    // Total number of frames (considering channels)
} sound;

//anything longer than ~10 seconds should be streamed
typedef struct music {
    audio_stream stream;         // Audio stream
    unsigned int frameCount;    // Total number of frames (considering channels)
    void *ctxData;
} music;

// Music context data, required for music streaming
typedef struct music_ctx {
    SNDFILE *snd_file;
    SRC_STATE *resampler;
    double src_ratio;
} music_ctx;

struct audio_buffer {
    float volume;                   // Audio buffer volume
    float pitch;                    // Audio buffer pitch
    float pan;                      // Audio buffer pan (0.0f to 1.0f)
    bool playing;                   // Audio buffer state: AUDIO_PLAYING
    bool paused;
    bool isStreaming;    // Audio buffer state: AUDIO_PAUSED
    bool isSubBufferProcessed[2];   // SubBuffer processed (virtual double buffer)
    unsigned int sizeInFrames;      // Total buffer size in frames
    unsigned int frameCursorPos;    // Frame cursor position
    unsigned int framesProcessed;   // Total frames processed in this buffer (required for play timing)
    unsigned char *data;            // Data buffer, on music stream keeps filling
    struct audio_buffer *next;             // Next audio buffer on the list
    struct audio_buffer *prev;             // Previous audio buffer on the list
};

typedef struct AudioData {
    struct {
        PaStream *stream;           // PortAudio stream
        PaStreamParameters outputParameters;  // Output stream parameters
        std::mutex lock;       // Mutex lock for thread synchronization
        bool isReady;               // Check if audio device is ready
        double sampleRate;
        size_t pcmBufferSize;       // Pre-allocated buffer size
        void *pcmBuffer;            // Pre-allocated buffer to read audio data from file/memory
        float masterVolume;         // Master volume control
    } System;
    struct {
        struct audio_buffer *first;         // Pointer to first audio_buffer in the list
        struct audio_buffer *last;          // Pointer to last audio_buffer in the list
    } Buffer;
} AudioData;

extern "C" {

void list_host_apis(void);
const char* get_host_api_name(PaHostApiIndex hostApi);
void init_audio_device(PaHostApiIndex host_api, double sample_rate, unsigned long buffer_size);
void close_audio_device(void);
bool is_audio_device_ready(void);
void set_master_volume(float volume);
float get_master_volume(void);

struct audio_buffer *load_audio_buffer(uint32_t channels, uint32_t size_in_frames, int usage);
void unload_audio_buffer(struct audio_buffer *buffer);
bool is_audio_buffer_playing(struct audio_buffer *buffer);
void play_audio_buffer(struct audio_buffer *buffer);
void stop_audio_buffer(struct audio_buffer *buffer);
void pause_audio_buffer(struct audio_buffer *buffer);
void resume_audio_buffer(struct audio_buffer *buffer);
void set_audio_buffer_volume(struct audio_buffer *buffer, float volume);
void set_audio_buffer_pitch(struct audio_buffer *buffer, float pitch);
void set_audio_buffer_pan(struct audio_buffer *buffer, float pan);
void track_audio_buffer(struct audio_buffer *buffer);
void untrack_audio_buffer(struct audio_buffer *buffer);

wave load_wave(const char* filename);
bool is_wave_valid(wave wave);
void unload_wave(wave wave);

sound load_sound_from_wave(wave wave);
sound load_sound(const char* filename);
bool is_sound_valid(sound sound);
void unload_sound(sound sound);
void play_sound(sound sound);
void pause_sound(sound sound);
void resume_sound(sound sound);
void stop_sound(sound sound);
bool is_sound_playing(sound sound);
void set_sound_volume(sound sound, float volume);
void set_sound_pitch(sound sound, float pitch);
void set_sound_pan(sound sound, float pan);

audio_stream load_audio_stream(unsigned int sample_rate, unsigned int sample_size, unsigned int channels);
void unload_audio_stream(audio_stream stream);
void play_audio_stream(audio_stream stream);
void pause_audio_stream(audio_stream stream);
void resume_audio_stream(audio_stream stream);
bool is_audio_stream_playing(audio_stream stream);
void stop_audio_stream(audio_stream stream);
void set_audio_stream_volume(audio_stream stream, float volume);
void set_audio_stream_pitch(audio_stream stream, float pitch);
void set_audio_stream_pan(audio_stream stream, float pan);
void update_audio_stream(audio_stream stream, const void *data, int frame_count);

music load_music_stream(const char* filename);
bool is_music_valid(music music);
void unload_music_stream(music music);
void play_music_stream(music music);
void pause_music_stream(music music);
void resume_music_stream(music music);
void stop_music_stream(music music);
void seek_music_stream(music music, float position);
bool music_stream_needs_update(music music);
void update_music_stream(music music);
bool is_music_stream_playing(music music);
void set_music_volume(music music, float volume);
void set_music_pitch(music music, float pitch);
void set_music_pan(music music, float pan);
float get_music_time_length(music music);
float get_music_time_played(music music);

} // extern "C"

// Global audio data (uses C++ std::mutex, so must be outside extern "C")
static AudioData AUDIO = {
    .System = {
        .masterVolume = 1.0f
    }
};

// Callback function (uses C++ features, so must have C++ linkage)
static int port_audio_callback(const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData)
{
    (void) inputBuffer;
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    float *out = (float*)outputBuffer;

    AUDIO.System.lock.lock();

    // Initialize output buffer with silence
    for (unsigned long i = 0; i < framesPerBuffer * AUDIO_DEVICE_CHANNELS; i++) {
        out[i] = 0.0f;
    }

    struct audio_buffer *audio_buffer = AUDIO.Buffer.first;
    while (audio_buffer != NULL) {
        if (audio_buffer->playing && !audio_buffer->paused && audio_buffer->data != NULL) {
            unsigned int subBufferSizeFrames = audio_buffer->sizeInFrames / 2;
            unsigned long framesToMix = framesPerBuffer;
            float *buffer_data = (float *)audio_buffer->data;

            while (framesToMix > 0) {
                unsigned int currentSubBufferIndex = (audio_buffer->frameCursorPos / subBufferSizeFrames) % 2;
                unsigned int frameOffsetInSubBuffer = audio_buffer->frameCursorPos % subBufferSizeFrames;
                unsigned int framesLeftInSubBuffer = subBufferSizeFrames - frameOffsetInSubBuffer;
                unsigned int framesThisPass = (framesToMix < framesLeftInSubBuffer) ? framesToMix : framesLeftInSubBuffer;

                if (audio_buffer->isSubBufferProcessed[currentSubBufferIndex]) {
                    // This part of the buffer is not ready, output silence
                } else {
                    // Calculate pan gains (0.0 = full left, 0.5 = center, 1.0 = full right)
                    float left_gain = sqrtf(1.0f - audio_buffer->pan);
                    float right_gain = sqrtf(audio_buffer->pan);

                    for (unsigned long i = 0; i < framesThisPass; i++) {
                        unsigned long buffer_pos = ((audio_buffer->frameCursorPos + i) % audio_buffer->sizeInFrames) * AUDIO_DEVICE_CHANNELS;
                        unsigned long output_pos = (framesPerBuffer - framesToMix + i) * AUDIO_DEVICE_CHANNELS;

                        for (int ch = 0; ch < AUDIO_DEVICE_CHANNELS; ch++) {
                            float sample = buffer_data[buffer_pos + ch] * audio_buffer->volume;
                            float gain = (ch == 0) ? left_gain : right_gain;
                            out[output_pos + ch] += sample * gain;
                        }
                    }
                }

                audio_buffer->frameCursorPos += framesThisPass;
                audio_buffer->framesProcessed += framesThisPass;
                framesToMix -= framesThisPass;

                unsigned int newSubBufferIndex = (audio_buffer->frameCursorPos / subBufferSizeFrames) % 2;
                if (newSubBufferIndex != currentSubBufferIndex) {
                    audio_buffer->isSubBufferProcessed[currentSubBufferIndex] = true;
                }

                if (!audio_buffer->isStreaming && audio_buffer->frameCursorPos >= audio_buffer->sizeInFrames) {
                    audio_buffer->playing = false;
                    break;
                }
            }
        }
        audio_buffer = audio_buffer->next;
    }

    for (unsigned long i = 0; i < framesPerBuffer * AUDIO_DEVICE_CHANNELS; i++) {
        out[i] *= AUDIO.System.masterVolume;
    }

    AUDIO.System.lock.unlock();

    return paContinue;
}

extern "C" {

void list_host_apis(void)
{
    PaHostApiIndex hostApiCount = Pa_GetHostApiCount();
    if (hostApiCount < 0) {
        TRACELOG(LOG_WARNING, "Failed to get host API count: %s", Pa_GetErrorText(hostApiCount));
        return;
    }

    TRACELOG(LOG_INFO, "Available host APIs:");
    for (PaHostApiIndex i = 0; i < hostApiCount; i++) {
        const PaHostApiInfo *info = Pa_GetHostApiInfo(i);
        if (info) {
            TRACELOG(LOG_INFO, "    [%d] %s (%d devices)", i, info->name, info->deviceCount);
        }
    }
}

const char* get_host_api_name(PaHostApiIndex hostApi)
{
    const PaHostApiInfo *hostApiInfo = Pa_GetHostApiInfo(hostApi);
    if (!hostApiInfo) {
        return NULL;
    }

    return hostApiInfo->name;
}

PaDeviceIndex get_best_output_device_for_host_api(PaHostApiIndex hostApi)
{
    const PaHostApiInfo *hostApiInfo = Pa_GetHostApiInfo(hostApi);
    if (!hostApiInfo) {
        return paNoDevice;
    }

    if (hostApiInfo->defaultOutputDevice != paNoDevice) {
        return hostApiInfo->defaultOutputDevice;
    }

    for (int i = 0; i < hostApiInfo->deviceCount; i++) {
        PaDeviceIndex deviceIndex = Pa_HostApiDeviceIndexToDeviceIndex(hostApi, i);
        if (deviceIndex >= 0) {
            const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(deviceIndex);
            if (deviceInfo && deviceInfo->maxOutputChannels > 0) {
                return deviceIndex;
            }
        }
    }

    return paNoDevice;
}

void init_audio_device(PaHostApiIndex host_api, double sample_rate, unsigned long buffer_size)
{
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        TRACELOG(LOG_WARNING, "Failed to initialize PortAudio: %s", Pa_GetErrorText(err));
        return;
    }

    AUDIO.System.outputParameters.device = get_best_output_device_for_host_api(host_api);
    if (AUDIO.System.outputParameters.device == paNoDevice) {
        TRACELOG(LOG_WARNING, "No usable output device found");
        Pa_Terminate();
        return;
    }

    AUDIO.System.outputParameters.channelCount = AUDIO_DEVICE_CHANNELS;
    AUDIO.System.outputParameters.sampleFormat = paFloat32; // Using float format like miniaudio version
    AUDIO.System.outputParameters.suggestedLatency = Pa_GetDeviceInfo(AUDIO.System.outputParameters.device)->defaultLowOutputLatency;
    AUDIO.System.outputParameters.hostApiSpecificStreamInfo = NULL;
    AUDIO.System.sampleRate = sample_rate;

    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(AUDIO.System.outputParameters.device);
    const PaHostApiInfo *hostApiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);

#ifdef _WIN32
    if (hostApiInfo->type == paASIO) {
        long minSize, maxSize, preferredSize, granularity;
        PaError asioErr = PaAsio_GetAvailableBufferSizes(AUDIO.System.outputParameters.device,
                                                          &minSize, &maxSize, &preferredSize, &granularity);

        if (asioErr == paNoError) {
            TRACELOG(LOG_INFO, "ASIO buffer size constraints:");
            TRACELOG(LOG_INFO, "    > Minimum:       %ld samples", minSize);
            TRACELOG(LOG_INFO, "    > Maximum:       %ld samples", maxSize);
            TRACELOG(LOG_INFO, "    > Preferred:     %ld samples", preferredSize);
            if (granularity == -1) {
                TRACELOG(LOG_INFO, "    > Granularity:   Powers of 2 only");
            } else if (granularity == 0) {
                TRACELOG(LOG_INFO, "    > Granularity:   Fixed size (min=max=preferred)");
            } else {
                TRACELOG(LOG_INFO, "    > Granularity:   %ld samples", granularity);
            }

            // Warn if requested buffer size is out of range
            if (buffer_size > 0 && buffer_size < minSize) {
                TRACELOG(LOG_WARNING, "Requested buffer size (%lu) is below ASIO minimum (%ld)", buffer_size, minSize);
                TRACELOG(LOG_WARNING, "Driver will use %ld samples instead", minSize);
            } else if (buffer_size > maxSize) {
                TRACELOG(LOG_WARNING, "Requested buffer size (%lu) exceeds ASIO maximum (%ld)", buffer_size, maxSize);
                TRACELOG(LOG_WARNING, "Driver will use %ld samples instead", maxSize);
            } else if (buffer_size == 0) {
                TRACELOG(LOG_INFO, "Buffer size not specified, driver will choose (likely %ld samples)", preferredSize);
            }
        } else {
            TRACELOG(LOG_WARNING, "Failed to query ASIO buffer sizes: %s", Pa_GetErrorText(asioErr));
        }
    }
#endif

    err = Pa_OpenStream(&AUDIO.System.stream,
                        NULL,                               // No input
                        &AUDIO.System.outputParameters,     // Output parameters
                        sample_rate,          // Sample rate
                        buffer_size,      // Frames per buffer
                        paNoFlag,                         // No clipping
                        port_audio_callback,                 // Callback function
                        NULL);                             // User data

    if (err != paNoError) {
        TRACELOG(LOG_WARNING, "Failed to open audio stream: %s", Pa_GetErrorText(err));
        Pa_Terminate();
        return;
    }

    err = Pa_StartStream(AUDIO.System.stream);
    if (err != paNoError) {
        TRACELOG(LOG_WARNING, "Failed to start audio stream: %s", Pa_GetErrorText(err));
        Pa_CloseStream(AUDIO.System.stream);
        Pa_Terminate();
        return;
    }

    AUDIO.System.isReady = true;

    TRACELOG(LOG_INFO, "Device initialized successfully");
    TRACELOG(LOG_INFO, "    > Backend:       PortAudio | %s", hostApiInfo->name);
    TRACELOG(LOG_INFO, "    > Device:        %s", deviceInfo->name);
    TRACELOG(LOG_INFO, "    > Format:        %s", "Float32");
    const PaStreamInfo *streamInfo = Pa_GetStreamInfo(AUDIO.System.stream);
    TRACELOG(LOG_INFO, "    > Channels:      %d", AUDIO_DEVICE_CHANNELS);
    TRACELOG(LOG_INFO, "    > Sample rate:   %f", AUDIO.System.sampleRate);
    TRACELOG(LOG_INFO, "    > Buffer size:   %lu (requested)", buffer_size);
    TRACELOG(LOG_INFO, "    > Latency:       %f ms", streamInfo->outputLatency * 1000.0);
#ifdef _WIN32
    if (hostApiInfo->type == paASIO) {
        unsigned long estimatedBufferSize = (unsigned long)(streamInfo->outputLatency * AUDIO.System.sampleRate);
        TRACELOG(LOG_INFO, "    > Estimated actual buffer: ~%lu samples (based on latency)", estimatedBufferSize);
        if (buffer_size > 0 && estimatedBufferSize != buffer_size) {
            TRACELOG(LOG_INFO, "    > Note:          ASIO driver adjusted buffer size to meet its constraints");
        }
    }
#endif
}

void close_audio_device(void)
{
    if (AUDIO.System.isReady) {
        PaError err = Pa_StopStream(AUDIO.System.stream);
        if (err != paNoError) {
            TRACELOG(LOG_WARNING, "Error stopping stream: %s", Pa_GetErrorText(err));
        }

        err = Pa_CloseStream(AUDIO.System.stream);
        if (err != paNoError) {
            TRACELOG(LOG_WARNING, "Error closing stream: %s", Pa_GetErrorText(err));
        }

        Pa_Terminate();

        AUDIO.System.isReady = false;
        FREE(AUDIO.System.pcmBuffer);
        AUDIO.System.pcmBuffer = NULL;
        AUDIO.System.pcmBufferSize = 0;

        TRACELOG(LOG_INFO, "Device closed successfully");
    }
    else {
        TRACELOG(LOG_WARNING, "Device could not be closed, not currently initialized");
    }
}

bool is_audio_device_ready(void)
{
    return AUDIO.System.isReady;
}

void set_master_volume(float volume)
{
    AUDIO.System.lock.lock();
    AUDIO.System.masterVolume = volume;
    AUDIO.System.lock.unlock();
}

float get_master_volume(void)
{
    AUDIO.System.lock.lock();
    float volume = AUDIO.System.masterVolume;
    AUDIO.System.lock.unlock();
    return volume;
}

struct audio_buffer *load_audio_buffer(uint32_t channels, uint32_t size_in_frames, int usage)
{
    struct audio_buffer *buffer = (struct audio_buffer*)calloc(1, sizeof(struct audio_buffer));

    if (buffer == NULL) {
        TRACELOG(LOG_WARNING, "Failed to allocate memory for buffer");
        return NULL;
    }

    buffer->data = (unsigned char*)calloc(size_in_frames * channels, sizeof(float));
    if (buffer->data == NULL) {
        TRACELOG(LOG_WARNING, "Failed to allocate memory for buffer data");
        FREE(buffer);
        return NULL;
    }

    buffer->volume = 1.0f;
    buffer->pitch = 1.0f;
    buffer->pan = 0.5f;
    buffer->playing = false;
    buffer->paused = false;
    buffer->frameCursorPos = 0;
    buffer->framesProcessed = 0;
    buffer->sizeInFrames = size_in_frames;
    if (usage == 0) { // Static buffer
        buffer->isSubBufferProcessed[0] = false;
        buffer->isSubBufferProcessed[1] = false;
    } else { // Streaming buffer
        buffer->isSubBufferProcessed[0] = true;
        buffer->isSubBufferProcessed[1] = true;
    }

    buffer->isStreaming = (usage == 1); //1 means streaming

    track_audio_buffer(buffer);

    return buffer;
}

void unload_audio_buffer(struct audio_buffer *buffer)
{
    if (buffer == NULL) return;

    untrack_audio_buffer(buffer);

    FREE(buffer->data);
    FREE(buffer);
}

bool is_audio_buffer_playing(struct audio_buffer *buffer)
{
    if (buffer == NULL) return false;

    AUDIO.System.lock.lock();
    bool result = (buffer->playing && !buffer->paused);
    AUDIO.System.lock.unlock();
    return result;
}

void play_audio_buffer(struct audio_buffer *buffer) {
    if (buffer == NULL) return;

    AUDIO.System.lock.lock();
    buffer->playing = true;
    buffer->paused = false;
    buffer->frameCursorPos = 0;
    buffer->framesProcessed = 0;
    if (!buffer->isStreaming) {
        buffer->isSubBufferProcessed[0] = false;
        buffer->isSubBufferProcessed[1] = false;
    }
    AUDIO.System.lock.unlock();
}

void stop_audio_buffer(struct audio_buffer* buffer) {
    if (buffer == NULL) return;

    AUDIO.System.lock.lock();
    buffer->playing = false;
    buffer->paused = false;
    buffer->frameCursorPos = 0;
    buffer->framesProcessed = 0;
    buffer->isSubBufferProcessed[0] = true;
    buffer->isSubBufferProcessed[1] = true;
    AUDIO.System.lock.unlock();
}

void pause_audio_buffer(struct audio_buffer* buffer) {
    if (buffer == NULL) return;

    AUDIO.System.lock.lock();
    buffer->paused = true;
    AUDIO.System.lock.unlock();
}

void resume_audio_buffer(struct audio_buffer* buffer) {
    if (buffer == NULL) return;

    AUDIO.System.lock.lock();
    buffer->paused = false;
    AUDIO.System.lock.unlock();
}

void set_audio_buffer_volume(struct audio_buffer* buffer, float volume) {
    if (buffer == NULL) return;

    AUDIO.System.lock.lock();
    buffer->volume = volume;
    AUDIO.System.lock.unlock();
}

void set_audio_buffer_pitch(struct audio_buffer* buffer, float pitch) {
    if ((buffer == NULL) || (pitch < 0.0f)) return;

    AUDIO.System.lock.lock();
    buffer->pitch = pitch;
    AUDIO.System.lock.unlock();
}

void set_audio_buffer_pan(struct audio_buffer* buffer, float pan) {
    if (buffer == NULL) return;
    if (pan < 0.0f) pan = 0.0f;
    else if (pan > 1.0f) pan = 1.0f;

    AUDIO.System.lock.lock();
    buffer->pan = pan;
    AUDIO.System.lock.unlock();
}

void track_audio_buffer(struct audio_buffer* buffer) {
    if (buffer == NULL) return;

    AUDIO.System.lock.lock();
    if (AUDIO.Buffer.first == NULL) AUDIO.Buffer.first = buffer;
    else {
        AUDIO.Buffer.last->next = buffer;
        buffer->prev = AUDIO.Buffer.last;
    }
    AUDIO.Buffer.last = buffer;
    AUDIO.System.lock.unlock();
}

void untrack_audio_buffer(struct audio_buffer* buffer) {
    if (buffer == NULL) return;

    AUDIO.System.lock.lock();
    if (buffer->prev == NULL) AUDIO.Buffer.first = buffer->next;
    else buffer->prev->next = buffer->next;

    if (buffer->next == NULL) AUDIO.Buffer.last = buffer->prev;
    else buffer->next->prev = buffer->prev;

    buffer->prev = NULL;
    buffer->next = NULL;
    AUDIO.System.lock.unlock();
}

wave load_wave(const char* filename) {
    wave wave = { 0 };
    SNDFILE *snd_file;
    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(sf_info));

/*#ifdef _WIN32
    // Convert UTF-8 filename to wide string for Windows
    int wlen = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
    if (wlen == 0) {
        TRACELOG(LOG_ERROR, "Failed to convert filename to wide string: '%s'\n", filename);
        return wave;
    }

    wchar_t *wfilename = new wchar_t[wlen];
    if (wfilename == NULL) {
        TRACELOG(LOG_ERROR, "Failed to allocate memory for wide filename");
        return wave;
    }

    MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename, wlen);
    snd_file = sf_wchar_open(wfilename, SFM_READ, &sf_info);
    free(wfilename);
#else*/
    snd_file = sf_open(filename, SFM_READ, &sf_info);
//#endif
    if (snd_file == NULL) {
        TRACELOG(LOG_ERROR, "Failed to open file '%s'", filename);
        TRACELOG(LOG_ERROR, "libsndfile error: %s", sf_strerror(NULL));
        return wave;
    }
    wave.frameCount = (unsigned int)sf_info.frames;
    wave.sampleRate = (unsigned int)sf_info.samplerate;
    wave.channels = (unsigned int)sf_info.channels;
    wave.sampleSize = 32; // Using 32-bit float samples

    size_t total_samples = sf_info.frames * sf_info.channels;
    wave.data = malloc(total_samples * sizeof(float));
    if (wave.data == NULL) {
        TRACELOG(LOG_ERROR, "Failed to allocate memory for wave data");
        sf_close(snd_file);
        return wave;
    }
    sf_readf_float(snd_file, (float*)wave.data, sf_info.frames);
    sf_close(snd_file);
    return wave;
}

bool is_wave_valid(wave wave) {
    bool result = false;
    if ((wave.data != NULL) &&      // Validate wave data available
        (wave.frameCount > 0) &&    // Validate frame count
        (wave.sampleRate > 0) &&    // Validate sample rate is supported
        (wave.sampleSize > 0) &&    // Validate sample size is supported
        (wave.channels > 0)) result = true; // Validate number of channels supported

    return result;
}

void unload_wave(wave wave) {
    FREE(wave.data);
}

sound load_sound_from_wave(wave wave) {
    sound sound = { 0 };
    if (wave.data == NULL) return sound;
    struct wave resampled_wave = { 0 };
    bool is_resampled = false;

    if (wave.sampleRate != AUDIO.System.sampleRate) {
        TRACELOG(LOG_INFO, "Resampling wave from %d Hz to %f Hz", wave.sampleRate, AUDIO.System.sampleRate);

        double src_ratio = AUDIO.System.sampleRate / (double)wave.sampleRate;
        long output_frames = (long)(wave.frameCount * src_ratio) + 10;

        resampled_wave.data = calloc(output_frames * wave.channels, sizeof(float));
        if (resampled_wave.data == NULL) {
            TRACELOG(LOG_WARNING, "Failed to allocate memory for resampling");
            return sound;
        }

        SRC_DATA src_data;
        src_data.data_in = (float*)wave.data;
        src_data.data_out = (float*)resampled_wave.data;
        src_data.input_frames = wave.frameCount;
        src_data.output_frames = output_frames;
        src_data.src_ratio = src_ratio;

        int error = src_simple(&src_data, SRC_SINC_FASTEST, wave.channels);

        if (error) {
            TRACELOG(LOG_WARNING, "Resampling failed: %s", src_strerror(error));
            FREE(resampled_wave.data);
            return sound;
        }

        resampled_wave.frameCount = src_data.output_frames_gen;
        resampled_wave.sampleRate = (int)AUDIO.System.sampleRate;
        resampled_wave.channels = wave.channels;
        resampled_wave.sampleSize = wave.sampleSize;
        is_resampled = true;
    }

    struct wave *wave_to_load = is_resampled ? &resampled_wave : &wave;
    struct audio_buffer *buffer = load_audio_buffer(AUDIO_DEVICE_CHANNELS, wave_to_load->frameCount, 0);

    if (buffer != NULL && buffer->data != NULL) {
        size_t samples_to_copy = wave_to_load->frameCount * wave_to_load->channels;
        size_t buffer_samples = wave_to_load->frameCount * AUDIO_DEVICE_CHANNELS;
        float *wave_data = (float *)wave_to_load->data;
        float *buffer_data = (float *)buffer->data;

        if (wave_to_load->channels == 1 && AUDIO_DEVICE_CHANNELS == 2) {
            for (unsigned int i = 0; i < wave_to_load->frameCount; i++) {
                buffer_data[i * 2] = wave_data[i];     // Left channel
                buffer_data[i * 2 + 1] = wave_data[i]; // Right channel
            }
        } else if (wave_to_load->channels == 2 && AUDIO_DEVICE_CHANNELS == 2) {
            memcpy(buffer_data, wave_data, samples_to_copy * sizeof(float));
        } else {
            size_t min_samples = (samples_to_copy < buffer_samples) ? samples_to_copy : buffer_samples;
            memcpy(buffer_data, wave_data, min_samples * sizeof(float));
        }
    }

    sound.frameCount = wave_to_load->frameCount;
    sound.stream.sampleRate = wave_to_load->sampleRate;
    sound.stream.sampleSize = wave_to_load->sampleSize;
    sound.stream.channels = wave_to_load->channels;
    sound.stream.buffer = buffer;

    if (is_resampled) {
        FREE(resampled_wave.data);
    }

    return sound;
}

sound load_sound(const char* filename) {
    wave wave = load_wave(filename);

    sound sound = load_sound_from_wave(wave);

    unload_wave(wave);
    return sound;
}

bool is_sound_valid(sound sound) {
    bool result = false;
    if ((sound.stream.buffer != NULL) &&      // Validate wave data available
        (sound.frameCount > 0) &&    // Validate frame count
        (sound.stream.sampleRate > 0) &&    // Validate sample rate is supported
        (sound.stream.sampleSize > 0) &&    // Validate sample size is supported
        (sound.stream.channels > 0)) result = true; // Validate number of channels supported

    return result;
}

void unload_sound(sound sound) {
    unload_audio_buffer(sound.stream.buffer);
}

void play_sound(sound sound) {
    play_audio_buffer(sound.stream.buffer);
}

void pause_sound(sound sound) {
    pause_audio_buffer(sound.stream.buffer);
}

void resume_sound(sound sound) {
    resume_audio_buffer(sound.stream.buffer);
}

void stop_sound(sound sound) {
    stop_audio_buffer(sound.stream.buffer);
}

bool is_sound_playing(sound sound) {
    return is_audio_buffer_playing(sound.stream.buffer);
}

void set_sound_volume(sound sound, float volume) {
    set_audio_buffer_volume(sound.stream.buffer, volume);
}

void set_sound_pitch(sound sound, float pitch) {
    set_audio_buffer_pitch(sound.stream.buffer, pitch);
}

void set_sound_pan(sound sound, float pan) {
    set_audio_buffer_pan(sound.stream.buffer, pan);
}

audio_stream load_audio_stream(unsigned int sample_rate, unsigned int sample_size, unsigned int channels)
{
    audio_stream stream = { 0 };

    stream.sampleRate = sample_rate;
    stream.sampleSize = sample_size;
    stream.channels = channels;

    stream.buffer = load_audio_buffer(AUDIO_DEVICE_CHANNELS, AUDIO.System.sampleRate, 1);
    return stream;
}

void unload_audio_stream(audio_stream stream)
{
    unload_audio_buffer(stream.buffer);
}

void play_audio_stream(audio_stream stream)
{
    play_audio_buffer(stream.buffer);
}

void pause_audio_stream(audio_stream stream)
{
    pause_audio_buffer(stream.buffer);
}

void resume_audio_stream(audio_stream stream)
{
    resume_audio_buffer(stream.buffer);
}

bool is_audio_stream_playing(audio_stream stream)
{
    return is_audio_buffer_playing(stream.buffer);
}

void stop_audio_stream(audio_stream stream)
{
    stop_audio_buffer(stream.buffer);
}

void set_audio_stream_volume(audio_stream stream, float volume)
{
    set_audio_buffer_volume(stream.buffer, volume);
}

void set_audio_stream_pitch(audio_stream stream, float pitch)
{
    set_audio_buffer_pitch(stream.buffer, pitch);
}

void set_audio_stream_pan(audio_stream stream, float pan)
{
    set_audio_buffer_pan(stream.buffer, pan);
}

void update_audio_stream(audio_stream stream, const void *data, int frame_count)
{
    if (stream.buffer == NULL || data == NULL) return;

    AUDIO.System.lock.lock();

    if (stream.buffer->data != NULL) {
        float *buffer_data = (float *)stream.buffer->data;
        const float *input_data = (const float *)data;

        unsigned int samples_to_copy = frame_count * AUDIO_DEVICE_CHANNELS;
        unsigned int max_samples = stream.buffer->sizeInFrames * AUDIO_DEVICE_CHANNELS;

        if (samples_to_copy > max_samples) {
            samples_to_copy = max_samples;
        }

        memcpy(buffer_data, input_data, samples_to_copy * sizeof(float));

        stream.buffer->sizeInFrames = frame_count;
    }

    AUDIO.System.lock.unlock();
}

music load_music_stream(const char* filename) {
    music music = { 0 };
    bool music_loaded = false;
    SF_INFO sf_info = { 0 };
    SNDFILE *snd_file;

/*#ifdef _WIN32
    // Convert UTF-8 filename to wide string for Windows
    int wlen = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
    if (wlen == 0) {
        TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to convert filename to wide string", filename);
        return music;
    }

    wchar_t *wfilename = new wchar_t[wlen];
    if (wfilename == NULL) {
        TRACELOG(LOG_WARNING, "FILEIO: Failed to allocate memory for wide filename");
        return music;
    }

    MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename, wlen);
    snd_file = sf_wchar_open(wfilename, SFM_READ, &sf_info);
    free(wfilename);
#else*/
    snd_file = sf_open(filename, SFM_READ, &sf_info);
//#endif

    if (snd_file != NULL) {
        music_ctx *ctx = (music_ctx *)calloc(1, sizeof(music_ctx));
        if (ctx == NULL) {
            TRACELOG(LOG_WARNING, "Failed to allocate memory for music context");
            sf_close(snd_file);
            return music;
        }
        ctx->snd_file = snd_file;
        if (sf_info.samplerate != AUDIO.System.sampleRate) {
            TRACELOG(LOG_INFO, "Resampling music from %d Hz to %f Hz", sf_info.samplerate, AUDIO.System.sampleRate);

            int error;
            ctx->resampler = src_new(SRC_SINC_FASTEST, sf_info.channels, &error);
            if (ctx->resampler == NULL) {
                TRACELOG(LOG_WARNING, "Failed to create resampler: %s", src_strerror(error));
                free(ctx);
                sf_close(snd_file);
                return music;
            }
            ctx->src_ratio = AUDIO.System.sampleRate / (double)sf_info.samplerate;
        } else {
            ctx->resampler = NULL;
            ctx->src_ratio = 1.0;
        }
        music.ctxData = ctx;
        int sample_size = 32;
        music.stream = load_audio_stream(AUDIO.System.sampleRate, sample_size, sf_info.channels);
        music.frameCount = (unsigned int)(sf_info.frames * ctx->src_ratio);
        music_loaded = true;
    }
    if (!music_loaded)
    {
        TRACELOG(LOG_WARNING, "FILEIO: [%s] Music file could not be opened", filename);
        if (snd_file) sf_close(snd_file);
    }
    else
    {
        TRACELOG(LOG_INFO, "FILEIO: [%s] Music file loaded successfully", filename);
        TRACELOG(LOG_INFO, "    > Sample rate:   %i Hz", music.stream.sampleRate);
        TRACELOG(LOG_INFO, "    > Sample size:   %i bits", music.stream.sampleSize);
        TRACELOG(LOG_INFO, "    > Channels:      %i (%s)", music.stream.channels,
                    (music.stream.channels == 1) ? "Mono" :
                    (music.stream.channels == 2) ? "Stereo" : "Multi");
        TRACELOG(LOG_INFO, "    > Total frames:  %i", music.frameCount);
    }
    return music;
}

bool is_music_valid(music music)
{
    return ((music.frameCount > 0) &&           // Validate audio frame count
            (music.stream.sampleRate > 0) &&    // Validate sample rate is supported
            (music.stream.sampleSize > 0) &&    // Validate sample size is supported
            (music.stream.channels > 0));       // Validate number of channels supported
}

void unload_music_stream(music music) {
    if (music.ctxData) {
        music_ctx *ctx = (music_ctx *)music.ctxData;
        if (ctx->snd_file) sf_close(ctx->snd_file);
        if (ctx->resampler) src_delete(ctx->resampler);
        free(ctx);
    }
    unload_audio_stream(music.stream);
}

void play_music_stream(music music) {
    play_audio_stream(music.stream);
}

void pause_music_stream(music music) {
    pause_audio_stream(music.stream);
}

void resume_music_stream(music music) {
    resume_audio_stream(music.stream);
}

void stop_music_stream(music music) {
    stop_audio_stream(music.stream);
}

void seek_music_stream(music music, float position) {
    if (music.stream.buffer == NULL || music.ctxData == NULL) return;

    music_ctx *ctx = (music_ctx *)music.ctxData;
    SNDFILE *sndFile = ctx->snd_file;
    unsigned int position_in_frames = (unsigned int)(position * music.stream.sampleRate / ctx->src_ratio);

    sf_count_t seek_result = sf_seek(sndFile, position_in_frames, SEEK_SET);
    if (seek_result < 0) return; // Seek failed

    AUDIO.System.lock.lock();
    music.stream.buffer->framesProcessed = position_in_frames;
    music.stream.buffer->frameCursorPos = 0; // Reset cursor
    music.stream.buffer->isSubBufferProcessed[0] = true;  // Force reload
    music.stream.buffer->isSubBufferProcessed[1] = true;  // Force reload
    AUDIO.System.lock.unlock();
}

bool music_stream_needs_update(music music) {
    if (music.stream.buffer == NULL || music.ctxData == NULL) return false;

    AUDIO.System.lock.lock();
    bool needs_update = music.stream.buffer->isSubBufferProcessed[0] ||
                        music.stream.buffer->isSubBufferProcessed[1];
    AUDIO.System.lock.unlock();

    return needs_update;
}

void update_music_stream(music music) {
    if (music.stream.buffer == NULL || music.ctxData == NULL) return;

    music_ctx *ctx = (music_ctx *)music.ctxData;
    SNDFILE *sndFile = ctx->snd_file;
    if (sndFile == NULL) return;

    bool needs_refill[2];
    AUDIO.System.lock.lock();
    needs_refill[0] = music.stream.buffer->isSubBufferProcessed[0];
    needs_refill[1] = music.stream.buffer->isSubBufferProcessed[1];
    AUDIO.System.lock.unlock();

    if (!needs_refill[0] && !needs_refill[1]) return;

    unsigned int subBufferSizeFrames = music.stream.buffer->sizeInFrames / 2;
    float *buffer_data = (float *)music.stream.buffer->data;
    bool needs_resampling = (ctx->resampler != NULL);
    bool needs_mono_to_stereo = (music.stream.channels == 1 && AUDIO_DEVICE_CHANNELS == 2);

    unsigned int frames_to_read = subBufferSizeFrames;
    if (needs_resampling) {
        frames_to_read = (unsigned int)(subBufferSizeFrames / ctx->src_ratio) + 1;
    }

    size_t required_size = frames_to_read * music.stream.channels * sizeof(float);
    if (AUDIO.System.pcmBufferSize < required_size) {
        FREE(AUDIO.System.pcmBuffer);
        AUDIO.System.pcmBuffer = calloc(1, required_size);
        AUDIO.System.pcmBufferSize = required_size;
    }

    for (int i = 0; i < 2; i++) {
        if (!needs_refill[i]) continue;

        sf_count_t frames_read = sf_readf_float(sndFile, (float*)AUDIO.System.pcmBuffer, frames_to_read);

        unsigned int subBufferOffset = i * subBufferSizeFrames * AUDIO_DEVICE_CHANNELS;
        float *input_ptr = (float *)AUDIO.System.pcmBuffer;
        sf_count_t frames_written = 0;

        if (needs_resampling) {
            SRC_DATA src_data;
            src_data.data_in = input_ptr;
            src_data.data_out = buffer_data + subBufferOffset;
            src_data.input_frames = frames_read;
            src_data.output_frames = subBufferSizeFrames;
            src_data.src_ratio = ctx->src_ratio;
            src_data.end_of_input = 0;  // Set to 1 on the last chunk if needed

            int error = src_process(ctx->resampler, &src_data);
            if (error) {
                TRACELOG(LOG_WARNING, "Resampling failed: %s", src_strerror(error));
            }

            frames_written = src_data.output_frames_gen;
        } else {
            if (needs_mono_to_stereo) {
                for (int j = 0; j < frames_read; j++) {
                    buffer_data[subBufferOffset + j*2] = input_ptr[j];
                    buffer_data[subBufferOffset + j*2 + 1] = input_ptr[j];
                }
            } else {
                memcpy(buffer_data + subBufferOffset, input_ptr, frames_read * music.stream.channels * sizeof(float));
            }
            frames_written = frames_read;
        }

        if (frames_written < subBufferSizeFrames) {
            unsigned int offset = subBufferOffset + (frames_written * AUDIO_DEVICE_CHANNELS);
            unsigned int size = (subBufferSizeFrames - frames_written) * AUDIO_DEVICE_CHANNELS * sizeof(float);
            memset(buffer_data + offset, 0, size);
        }
    }

    AUDIO.System.lock.lock();
    if (needs_refill[0]) music.stream.buffer->isSubBufferProcessed[0] = false;
    if (needs_refill[1]) music.stream.buffer->isSubBufferProcessed[1] = false;
    AUDIO.System.lock.unlock();
}

bool is_music_stream_playing(music music) {
    return is_audio_stream_playing(music.stream);
}

void set_music_volume(music music, float volume) {
    set_audio_stream_volume(music.stream, volume);
}

void set_music_pitch(music music, float pitch) {
    set_audio_buffer_pitch(music.stream.buffer, pitch);
}

void set_music_pan(music music, float pan) {
    set_audio_buffer_pan(music.stream.buffer, pan);
}

float get_music_time_length(music music) {
    float total_seconds = 0.0f;

    total_seconds = (float)music.frameCount/AUDIO.System.sampleRate;

    return total_seconds;
}

float get_music_time_played(music music) {
    float seconds_played = 0.0f;
    if (music.stream.buffer != NULL) {
        AUDIO.System.lock.lock();
        seconds_played = (float)music.stream.buffer->framesProcessed / AUDIO.System.sampleRate;
        AUDIO.System.lock.unlock();
    }
    return seconds_played;
}

} // extern "C"
