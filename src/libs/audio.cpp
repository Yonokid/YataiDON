#include "audio.h"
#ifdef __ANDROID__
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
}

// Decode any audio file to float32 interleaved via FFmpeg.
// Returns false on failure; on success fills frames/sample_rate/channels/data (caller frees).
static bool ffmpeg_decode_float(const char* path,
    float** out_data, sf_count_t* out_frames,
    unsigned int* out_rate, unsigned int* out_channels)
{
    AVFormatContext* fmt = nullptr;
    if (avformat_open_input(&fmt, path, nullptr, nullptr) < 0) return false;
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        avformat_close_input(&fmt); return false;
    }
    int audio_idx = -1;
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_idx = (int)i; break;
        }
    }
    if (audio_idx < 0) { avformat_close_input(&fmt); return false; }

    auto* par = fmt->streams[audio_idx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(par->codec_id);
    if (!codec) { avformat_close_input(&fmt); return false; }
    AVCodecContext* dec = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(dec, par);
    if (avcodec_open2(dec, codec, nullptr) < 0) {
        avcodec_free_context(&dec); avformat_close_input(&fmt); return false;
    }

    SwrContext* swr = nullptr;
    AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;
    int out_ch = 2;
    swr_alloc_set_opts2(&swr, &out_layout, AV_SAMPLE_FMT_FLT, dec->sample_rate,
                        &dec->ch_layout, dec->sample_fmt, dec->sample_rate, 0, nullptr);
    swr_init(swr);

    std::vector<float> pcm;
    AVPacket* pkt = av_packet_alloc();
    AVFrame*  frm = av_frame_alloc();

    auto drain = [&]() {
        while (true) {
            int r = avcodec_receive_frame(dec, frm);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) break;
            if (r < 0) break;
            int64_t n = swr_get_out_samples(swr, frm->nb_samples);
            std::vector<float> tmp(n * out_ch);
            uint8_t* op = (uint8_t*)tmp.data();
            int got = swr_convert(swr, &op, (int)n,
                                  (const uint8_t**)frm->extended_data, frm->nb_samples);
            if (got > 0) pcm.insert(pcm.end(), tmp.begin(), tmp.begin() + got * out_ch);
            av_frame_unref(frm);
        }
    };

    while (av_read_frame(fmt, pkt) >= 0) {
        if (pkt->stream_index == audio_idx) {
            avcodec_send_packet(dec, pkt);
            drain();
        }
        av_packet_unref(pkt);
    }
    avcodec_send_packet(dec, nullptr);
    drain();

    av_packet_free(&pkt);
    av_frame_free(&frm);
    swr_free(&swr);

    *out_rate    = (unsigned int)dec->sample_rate;
    *out_channels = (unsigned int)out_ch;
    *out_frames  = (sf_count_t)(pcm.size() / out_ch);
    *out_data    = new float[pcm.size()];
    std::copy(pcm.begin(), pcm.end(), *out_data);

    avcodec_free_context(&dec);
    avformat_close_input(&fmt);
    return true;
}
#endif // __ANDROID__

static sf_count_t vf_get_filelen(void* user_data) {
    auto* vf = static_cast<VirtualFile*>(user_data);
    return static_cast<sf_count_t>(vf->data->size());
}

static sf_count_t vf_seek(sf_count_t offset, int whence, void* user_data) {
    auto* vf = static_cast<VirtualFile*>(user_data);
    sf_count_t size = static_cast<sf_count_t>(vf->data->size());

    switch (whence) {
        case SEEK_SET: vf->pos = offset;             break;
        case SEEK_CUR: vf->pos += offset;            break;
        case SEEK_END: vf->pos = size + offset;      break;
        default:       return -1;
    }

    if (vf->pos < 0)    vf->pos = 0;
    if (vf->pos > size) vf->pos = size;
    return vf->pos;
}

static sf_count_t vf_read(void* ptr, sf_count_t count, void* user_data) {
    auto* vf   = static_cast<VirtualFile*>(user_data);
    sf_count_t remaining = static_cast<sf_count_t>(vf->data->size()) - vf->pos;
    sf_count_t to_read   = std::min(count, remaining);
    if (to_read <= 0) return 0;
    std::memcpy(ptr, vf->data->data() + vf->pos, static_cast<std::size_t>(to_read));
    vf->pos += to_read;
    return to_read;
}

static sf_count_t vf_write(const void*, sf_count_t, void*) {
    return 0;  // read-only
}

static sf_count_t vf_tell(void* user_data) {
    return static_cast<VirtualFile*>(user_data)->pos;
}

AudioEngine::AudioEngine()
{
}

AudioEngine::~AudioEngine() {
    if (is_ready) {
        close_audio_device();
    }
}

void AudioEngine::mix(float* out, unsigned int framesPerBuffer, AudioEngine* engine) {

    const unsigned long buffer_size = framesPerBuffer * 2;
    std::memset(out, 0, buffer_size * sizeof(float));

    if (!engine) return;

    std::shared_lock<std::shared_mutex> guard(engine->rw_lock);

    for (auto& [name, snd] : engine->sounds) {
        std::atomic_ref<bool>         aref_playing(snd.is_playing);
        std::atomic_ref<unsigned int> aref_frame(snd.current_frame);

        if (!aref_playing.load(std::memory_order_acquire)) continue;

        const float volume = std::atomic_ref<float>(snd.volume).load(std::memory_order_relaxed);
        const float pan    = std::atomic_ref<float>(snd.pan).load(std::memory_order_relaxed);
        unsigned int frame = aref_frame.load(std::memory_order_relaxed);

        const float pitch    = std::atomic_ref<float>(snd.pitch).load(std::memory_order_relaxed);
        const float* data_ptr = snd.data;
        const unsigned int channels = snd.channels;

        unsigned long frames_to_process = framesPerBuffer;
        unsigned long output_index = 0;
        bool still_playing = true;
        float frame_f = (float)frame;

        while (frames_to_process > 0 && still_playing) {
            unsigned long src_frame = (unsigned long)frame_f;
            if (src_frame >= snd.frame_count) {
                if (snd.loop) { frame_f = 0.0f; continue; }
                else { still_playing = false; break; }
            }

            unsigned long src_index = src_frame * channels;
            unsigned long dst_index = output_index * 2;
            float left, right;

            if (channels == 1) {
                float sample = data_ptr[src_index];
                left  = sample * (1.0f - pan);
                right = sample * pan;
            } else {
                left  = data_ptr[src_index];
                right = data_ptr[src_index + 1];
                if (pan < 0.5f)      right *= (pan * 2.0f);
                else if (pan > 0.5f) left  *= ((1.0f - pan) * 2.0f);
            }

            out[dst_index]     += left * volume;
            out[dst_index + 1] += right * volume;

            frame_f += pitch;
            output_index++;
            frames_to_process--;
        }
        frame = (unsigned int)frame_f;

        aref_frame.store(frame, std::memory_order_relaxed);
        if (!still_playing)
            aref_playing.store(false, std::memory_order_release);
    }

    for (auto& [name, mus] : engine->music_streams) {
        std::atomic_ref<bool> aref_playing(mus.is_playing);

        if (!aref_playing.load(std::memory_order_acquire)) continue;

        std::atomic_ref<unsigned long long> aref_frame(mus.current_frame);
        const float volume = std::atomic_ref<float>(mus.volume).load(std::memory_order_relaxed);
        const float pan    = std::atomic_ref<float>(mus.pan).load(std::memory_order_relaxed);

        unsigned long frames_to_process = framesPerBuffer;
        unsigned long output_index = 0;

        while (frames_to_process > 0 && aref_playing.load(std::memory_order_relaxed)) {
            if (mus.buffer_position >= mus.frames_in_buffer) {
                if (mus.file_handle) {
                    sf_count_t frames_read = sf_readf_float(mus.file_handle, mus.stream_buffer, mus.buffer_size);

                    if (frames_read == 0) {
                        if (mus.loop) {
                            sf_seek(mus.file_handle, 0, SEEK_SET);
                            frames_read = sf_readf_float(mus.file_handle, mus.stream_buffer, mus.buffer_size);
                            if (mus.resampler) {
                                src_reset(mus.resampler);
                            }
                        } else {
                            aref_playing.store(false, std::memory_order_release);
                            break;
                        }
                    }

                    if (mus.resampler && frames_read > 0) {
                        double ratio = engine->target_sample_rate / (double)mus.file_info.samplerate;

                        SRC_DATA src_data;
                        src_data.data_in = mus.stream_buffer;
                        src_data.input_frames = frames_read;
                        src_data.data_out = mus.resample_buffer;
                        src_data.output_frames = (long)(frames_read * ratio) + 256;
                        src_data.src_ratio = ratio;
                        src_data.end_of_input = 0;

                        int error = src_process(mus.resampler, &src_data);
                        if (error) {
                            spdlog::error("Resampling error for music stream {}: {}", name, src_strerror(error));
                            aref_playing.store(false, std::memory_order_release);
                            break;
                        }

                        mus.frames_in_buffer = src_data.output_frames_gen;
                    } else {
                        mus.frames_in_buffer = frames_read;
                    }

                    mus.buffer_position = 0;
                } else if (mus.pcm_data) {
                    unsigned long long frame_pos = aref_frame.load(std::memory_order_relaxed);
                    sf_count_t frames_left = (mus.pcm_total_frames > (sf_count_t)frame_pos)
                                             ? mus.pcm_total_frames - (sf_count_t)frame_pos : 0;
                    sf_count_t to_fill = std::min((sf_count_t)mus.buffer_size, frames_left);

                    if (to_fill == 0) {
                        if (mus.loop) {
                            aref_frame.store(0ULL, std::memory_order_relaxed);
                            frame_pos = 0;
                            to_fill = std::min((sf_count_t)mus.buffer_size, mus.pcm_total_frames);
                        } else {
                            aref_playing.store(false, std::memory_order_release);
                            break;
                        }
                    }

                    unsigned int ch = (unsigned int)mus.file_info.channels;
                    std::memcpy(mus.stream_buffer, mus.pcm_data + frame_pos * ch,
                                (size_t)to_fill * ch * sizeof(float));
                    mus.frames_in_buffer = (unsigned int)to_fill;
                    mus.buffer_position  = 0;
                } else {
                    aref_playing.store(false, std::memory_order_release);
                    break;
                }
            }

            unsigned long frames_available = mus.frames_in_buffer - mus.buffer_position;
            unsigned long frames_to_read = (frames_to_process < frames_available) ? frames_to_process : frames_available;

            const unsigned int channels = mus.file_info.channels;
            const float* source_buffer = mus.resampler ? mus.resample_buffer : mus.stream_buffer;

            for (unsigned long i = 0; i < frames_to_read; i++) {
                unsigned long src_index = (mus.buffer_position + i) * channels;
                unsigned long dst_index = (output_index + i) * 2;

                float left, right;

                if (channels == 1) {
                    float sample = source_buffer[src_index];
                    left  = sample * (1.0f - pan);
                    right = sample * pan;
                } else {
                    left  = source_buffer[src_index];
                    right = source_buffer[src_index + 1];
                    if (pan < 0.5f)      right *= (pan * 2.0f);
                    else if (pan > 0.5f) left  *= ((1.0f - pan) * 2.0f);
                }

                out[dst_index]     += left * volume;
                out[dst_index + 1] += right * volume;
            }

            mus.buffer_position += frames_to_read;
            aref_frame.fetch_add(frames_to_read, std::memory_order_relaxed);
            output_index      += frames_to_read;
            frames_to_process -= frames_to_read;
        }
    }

    guard.unlock();

    const float master_vol = engine->master_volume.load(std::memory_order_relaxed);
    for (unsigned long i = 0; i < buffer_size; i++) {
        float sample = out[i] * master_vol;
        out[i] = (sample > 1.0f) ? 1.0f : ((sample < -1.0f) ? -1.0f : sample);
    }

}

void AudioEngine::sdl_audio_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int /*total_amount*/) {
    AudioEngine* engine = static_cast<AudioEngine*>(userdata);
    if (!engine || additional_amount <= 0) return;

    const int bytes_per_frame = static_cast<int>(sizeof(float)) * 2; // f32 stereo, matches the spec we opened with
    unsigned int frames = static_cast<unsigned int>(additional_amount) / bytes_per_frame;
    if (frames == 0) return;

    size_t needed_floats = static_cast<size_t>(frames) * 2;
    if (engine->sdl_scratch_buffer.size() < needed_floats) {
        engine->sdl_scratch_buffer.resize(needed_floats);
    }

    mix(engine->sdl_scratch_buffer.data(), frames, engine);

    SDL_PutAudioStreamData(stream, engine->sdl_scratch_buffer.data(),
                            static_cast<int>(needed_floats * sizeof(float)));
}

#ifdef _WIN32
int AudioEngine::rt_audio_callback(void* outputBuffer, void* /*inputBuffer*/,
                                    unsigned int framesPerBuffer, double /*streamTime*/,
                                    unsigned int /*status*/, void* userData) {
    AudioEngine* engine = static_cast<AudioEngine*>(userData);
    mix(static_cast<float*>(outputBuffer), framesPerBuffer, engine);
    return 0;
}
#endif

static const char* sdl_driver_name_for(int device_type) {
    switch (device_type) {
        case 1: return "alsa";
        case 2: return "pulseaudio";
        case 3: return "jack";
        case 4: return "coreaudio";
        case 5: return "wasapi";
        case 7: return "directsound";
        default: return nullptr; // 0 = auto
    }
}

bool AudioEngine::init_audio_device(const fs::path& sounds_path, const AudioConfig& audio_config, const VolumeConfig& volume_presets) {
    this->sounds_path = sounds_path;
    this->target_sample_rate = audio_config.sample_rate < 0 ? 44100.0f : audio_config.sample_rate;
    this->buffer_size = audio_config.buffer_size;
    this->volume_presets = volume_presets;
    this->is_ready = false;
    this->master_volume = 1.0f;
    try {
#if defined(_WIN32)
        if (audio_config.device_type == 6) {
            // ASIO via RtAudio (Windows only)
            rt_audio = new RtAudio(RtAudio::WINDOWS_ASIO, [](RtAudioErrorType type, const std::string& errorText) {
                if (type == RTAUDIO_WARNING)
                    spdlog::warn("RtAudio ASIO: {}", errorText);
                else
                    spdlog::error("RtAudio ASIO: {}", errorText);
            });

            if (rt_audio->getDeviceCount() == 0) {
                spdlog::error("No ASIO devices found");
                delete rt_audio;
                rt_audio = nullptr;
                return false;
            }

            RtAudio::StreamParameters params;
            params.deviceId     = rt_audio->getDefaultOutputDevice();
            params.nChannels    = 2;
            params.firstChannel = 0;

            unsigned int bufferFrames = static_cast<unsigned int>(buffer_size);

            RtAudio::StreamOptions options;
            options.flags    = RTAUDIO_MINIMIZE_LATENCY | RTAUDIO_SCHEDULE_REALTIME;
            options.priority = 99;

            RtAudioErrorType err = rt_audio->openStream(&params, nullptr, RTAUDIO_FLOAT32,
                static_cast<unsigned int>(target_sample_rate), &bufferFrames,
                &AudioEngine::rt_audio_callback, this, &options);
            if (err != RTAUDIO_NO_ERROR) {
                spdlog::error("Failed to open ASIO stream: {}", rt_audio->getErrorText());
                delete rt_audio;
                rt_audio = nullptr;
                return false;
            }

            err = rt_audio->startStream();
            if (err != RTAUDIO_NO_ERROR) {
                spdlog::error("Failed to start ASIO stream: {}", rt_audio->getErrorText());
                rt_audio->closeStream();
                delete rt_audio;
                rt_audio = nullptr;
                return false;
            }

            is_ready = true;

            auto dev_info = rt_audio->getDeviceInfo(params.deviceId);
            spdlog::info("Audio Device initialized successfully");
            spdlog::info("    > Backend:       RtAudio | ASIO");
            spdlog::info("    > Device:        {}", dev_info.name);
            spdlog::info("    > Format:        Float32");
            spdlog::info("    > Channels:      2");
            spdlog::info("    > Sample rate:   {} Hz", rt_audio->getStreamSampleRate());
            spdlog::info("    > Buffer size:   {} frames (actual)", bufferFrames);
        } else if (audio_config.exclusive_mode &&
                   wdmks_exclusive::init(static_cast<unsigned int>(target_sample_rate),
                                          static_cast<unsigned int>(buffer_size),
                                          &AudioEngine::mix, this)) {
            is_ready = true;
            spdlog::info("Audio Device initialized successfully");
            spdlog::info("    > Backend:       WDM-KS (raw, kernel streaming)");
            spdlog::info("    > Format:        Float32, 2ch, {} Hz", (int)target_sample_rate);
            spdlog::info("    > Buffer size:   {} frames (requested)", buffer_size);
        } else
#endif
        {
            if (audio_config.exclusive_mode) {
                spdlog::warn("Exclusive mode unavailable; falling back to SDL3 shared mode");
            }

            const char* driver = sdl_driver_name_for(audio_config.device_type);
            if (driver) SDL_SetHint(SDL_HINT_AUDIO_DRIVER, driver);
            else        SDL_ResetHint(SDL_HINT_AUDIO_DRIVER);

            char frames_str[16];
            std::snprintf(frames_str, sizeof(frames_str), "%lu", buffer_size);
            SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, frames_str);

            if (!sdl_audio_subsystem_initialized) {
                if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
                    spdlog::error("Failed to init SDL audio subsystem: {}", SDL_GetError());
                    return false;
                }
                sdl_audio_subsystem_initialized = true;
            }

            SDL_AudioSpec spec{};
            spec.format   = SDL_AUDIO_F32;
            spec.channels = 2;
            spec.freq     = static_cast<int>(target_sample_rate);

            sdl_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                                    AudioEngine::sdl_audio_callback, this);
            if (!sdl_stream) {
                spdlog::error("Failed to open SDL audio device stream: {}", SDL_GetError());
                return false;
            }

            if (!SDL_ResumeAudioStreamDevice(sdl_stream)) {
                spdlog::error("Failed to start SDL audio stream: {}", SDL_GetError());
                SDL_DestroyAudioStream(sdl_stream);
                sdl_stream = nullptr;
                return false;
            }

            is_ready = true;

            SDL_AudioSpec actual_spec{};
            int actual_frames = 0;
            SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(sdl_stream), &actual_spec, &actual_frames);

            spdlog::info("Audio Device initialized successfully");
            spdlog::info("    > Backend:       SDL3 | {}", SDL_GetCurrentAudioDriver());
            spdlog::info("    > Format:        Float32");
            spdlog::info("    > Channels:      {} (requested 2)", actual_spec.channels);
            spdlog::info("    > Sample rate:   {} Hz (requested {} Hz)", actual_spec.freq, (int)target_sample_rate);
            spdlog::info("    > Buffer size:   {} frames (device-reported)", actual_frames);
        }

        return is_ready;
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize audio device: {}", e.what());
        return false;
    }
}

void AudioEngine::close_audio_device() {
    try {
        unload_all_sounds();
        unload_all_music();

        if (sdl_stream) {
            SDL_DestroyAudioStream(sdl_stream); // also closes the underlying device
            sdl_stream = nullptr;
        }
#ifdef _WIN32
        wdmks_exclusive::shutdown(); // idempotent no-op if not running
#endif
        if (sdl_audio_subsystem_initialized) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            sdl_audio_subsystem_initialized = false;
        }
#ifdef _WIN32
        if (rt_audio != nullptr) {
            if (rt_audio->isStreamRunning()) rt_audio->stopStream();
            if (rt_audio->isStreamOpen()) rt_audio->closeStream();
            delete rt_audio;
            rt_audio = nullptr;
        }
#endif
        is_ready = false;

        spdlog::info("Audio device closed");
    } catch (const std::exception& e) {
        spdlog::error("Error closing audio device: {}", e.what());
    }
}

bool AudioEngine::is_audio_device_ready() const {
    return is_ready;
}

void AudioEngine::set_master_volume(float volume) {
    master_volume.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
}

float AudioEngine::get_master_volume() {
    return master_volume.load(std::memory_order_relaxed);
}

std::string AudioEngine::path_to_string(const fs::path& path) const {
#ifdef _WIN32
    // Convert to ANSI codepage for Windows (CP932 for Japanese)
    std::wstring wpath = path.wstring();
    int size = WideCharToMultiByte(932, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size > 0) {
        std::string result(size - 1, '\0');
        WideCharToMultiByte(932, 0, wpath.c_str(), -1, &result[0], size, nullptr, nullptr);
        return result;
    }
#endif
    return path.string();
}

std::string AudioEngine::load_sound(const fs::path& file_path, const std::string& name) {
    try {
        SF_INFO file_info;
        std::memset(&file_info, 0, sizeof(SF_INFO));

        SNDFILE* file = nullptr;
        #ifdef _WIN32
        file = sf_wchar_open(file_path.wstring().c_str(), SFM_READ, &file_info);
        #endif

        if (!file) {
            std::string path_str = path_to_string(file_path);
            file = sf_open(path_str.c_str(), SFM_READ, &file_info);
        }

        if (!file) {
#ifdef __ANDROID__
            float* ff_data = nullptr;
            sf_count_t ff_frames = 0;
            unsigned int ff_rate = 0, ff_ch = 0;
            std::string path_str2 = path_to_string(file_path);
            if (!ffmpeg_decode_float(path_str2.c_str(), &ff_data, &ff_frames, &ff_rate, &ff_ch)) {
                spdlog::error("Failed to open sound file: {} - {} (ffmpeg fallback also failed)",
                              file_path.string(), sf_strerror(NULL));
                return "";
            }
            sound snd;
            snd.data        = ff_data;
            snd.frame_count = ff_frames;
            snd.sample_rate = ff_rate;
            snd.channels    = ff_ch;
            snd.is_playing  = false;
            snd.current_frame = 0;
            snd.loop   = false;
            snd.volume = 1.0f;
            snd.pan    = 0.5f;
            snd.pitch  = 1.0f;
            if ((double)ff_rate != target_sample_rate) {
                double ratio = target_sample_rate / (double)ff_rate;
                long out_frames = (long)(ff_frames * ratio) + 1;
                float* rs = new float[out_frames * ff_ch];
                SRC_DATA sd;
                sd.data_in = ff_data; sd.input_frames = ff_frames;
                sd.data_out = rs; sd.output_frames = out_frames;
                sd.src_ratio = ratio; sd.end_of_input = 1;
                int err = src_simple(&sd, SRC_SINC_FASTEST, (int)ff_ch);
                if (err) { delete[] ff_data; delete[] rs; return ""; }
                delete[] ff_data;
                snd.data = rs;
                snd.frame_count = sd.output_frames_gen;
                snd.sample_rate = (unsigned int)target_sample_rate;
            }
            snd.resampler = nullptr;
            snd.resample_buffer = nullptr;
            {
                std::unique_lock<std::shared_mutex> guard(rw_lock);
                sounds[name] = snd;
            }
            spdlog::debug("Loaded sound (ffmpeg): {} ({} frames, {} Hz, {} ch)",
                          name, snd.frame_count, snd.sample_rate, snd.channels);
            return name;
#else
            spdlog::error("Failed to open sound file: {} - {}", file_path.string(), sf_strerror(NULL));
            return "";
#endif
        }

        unsigned int total_frames = file_info.frames;
        unsigned int channels = file_info.channels;
        float* data = new float[total_frames * channels];

        sf_count_t frames_read = sf_readf_float(file, data, total_frames);
        sf_close(file);

        if (frames_read != total_frames) {
            spdlog::warn("Read {} frames, expected {} for sound: {}", frames_read, total_frames, file_path.string());
        }

        sound snd;
        snd.data = data;
        snd.frame_count = frames_read;
        snd.sample_rate = file_info.samplerate;
        snd.channels = channels;
        snd.is_playing = false;
        snd.current_frame = 0;
        snd.loop = false;
        snd.volume = 1.0f;
        snd.pan = 0.5f;
        snd.pitch = 1.0f;

        if (snd.sample_rate != target_sample_rate) {
            double ratio = target_sample_rate / (double)snd.sample_rate;
            long output_frames = (long)(frames_read * ratio) + 1;
            float* resampled_data = new float[output_frames * channels];

            SRC_DATA src_data;
            src_data.data_in = data;
            src_data.input_frames = frames_read;
            src_data.data_out = resampled_data;
            src_data.output_frames = output_frames;
            src_data.src_ratio = ratio;
            src_data.end_of_input = 1;

            int error = src_simple(&src_data, SRC_SINC_FASTEST, channels);
            if (error) {
                spdlog::error("Failed to resample sound: {} - {}", file_path.string(), src_strerror(error));
                delete[] data;
                delete[] resampled_data;
                return "";
            }

            delete[] data;
            snd.data = resampled_data;
            snd.frame_count = src_data.output_frames_gen;
            snd.sample_rate = target_sample_rate;
        }

        snd.resampler = nullptr;
        snd.resample_buffer = nullptr;

        {
            std::unique_lock<std::shared_mutex> guard(rw_lock);
            sounds[name] = snd;
        }

        spdlog::debug("Loaded sound: {} ({} frames, {} Hz, {} channels)",
                     name, snd.frame_count, snd.sample_rate, snd.channels);

        return name;

    } catch (const std::exception& e) {
        spdlog::error("Error loading sound {}: {}", file_path.string(), e.what());
        return "";
    }
}

void AudioEngine::load_screen_sounds(const std::string& screen_name) {
    fs::path path = sounds_path / screen_name;
    if (!fs::exists(path)) {
        spdlog::warn("Sounds for screen {} not found", screen_name);
        return;
    }

    load_sound(sounds_path / "don.wav", "don");
    load_sound(sounds_path / "ka.wav",  "kat");

    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_directory()) {
                for (const auto& file : fs::directory_iterator(entry.path())) {
                    load_sound(file.path(),
                                entry.path().stem().string() + "_" + file.path().stem().string());
                }
            } else if (entry.is_regular_file()) {
                load_sound(entry.path(), entry.path().stem().string());
            }
        }
    } catch (const fs::filesystem_error& e) {
        spdlog::error("load_screen_sounds: error scanning {}: {}", path.string(), e.what());
    }

    fs::path global_path = sounds_path / "global";
    if (fs::exists(global_path)) {
        try {
            for (const auto& entry : fs::directory_iterator(global_path)) {
                if (entry.is_directory()) {
                    for (const auto& file : fs::directory_iterator(entry.path())) {
                        load_sound(file.path(),
                                    entry.path().stem().string() + "_" + file.path().stem().string());
                    }
                } else if (entry.is_regular_file()) {
                    load_sound(entry.path(), entry.path().stem().string());
                }
            }
        } catch (const fs::filesystem_error& e) {
            spdlog::error("load_screen_sounds: error scanning global sounds: {}", e.what());
        }
    }
}

void AudioEngine::unload_sound(const std::string& name) {
    std::unique_lock<std::shared_mutex> guard(rw_lock);
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        sound& snd = it->second;

        std::atomic_ref<bool>(snd.is_playing).store(false, std::memory_order_relaxed);

        if (snd.data) {
            delete[] snd.data;
            snd.data = nullptr;
        }

        if (snd.resampler) {
            src_delete(snd.resampler);
            snd.resampler = nullptr;
        }

        if (snd.resample_buffer) {
            delete[] snd.resample_buffer;
            snd.resample_buffer = nullptr;
        }

        sounds.erase(it);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

void AudioEngine::unload_all_sounds() {
    std::vector<std::string> names;
    {
        std::shared_lock<std::shared_mutex> guard(rw_lock);
        names.reserve(sounds.size());
        for (const auto& [name, _] : sounds) names.push_back(name);
    }

    for (const auto& name : names) {
        unload_sound(name);
    }

    spdlog::info("All sounds unloaded");
}

void AudioEngine::play_sound(const std::string& name, VolumePreset volume_preset) {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        sound& snd = it->second;

        if (volume_preset != VolumePreset::NONE) {
            float volume = 1.0f;
            if      (volume_preset == VolumePreset::SOUND)       volume = volume_presets.sound;
            else if (volume_preset == VolumePreset::MUSIC)       volume = volume_presets.music;
            else if (volume_preset == VolumePreset::VOICE)       volume = volume_presets.voice;
            else if (volume_preset == VolumePreset::HITSOUND)    volume = volume_presets.hitsound;
            else if (volume_preset == VolumePreset::ATTRACT_MODE) volume = volume_presets.attract_mode;
            std::atomic_ref<float>(snd.volume).store(volume, std::memory_order_relaxed);
        }

        std::atomic_ref<unsigned int>(snd.current_frame).store(0, std::memory_order_relaxed);
        std::atomic_ref<bool>(snd.is_playing).store(true, std::memory_order_release);
    } else {
        //spdlog::warn("Sound {} not found", name);
    }
}

void AudioEngine::stop_sound(const std::string& name) {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        std::atomic_ref<bool>(it->second.is_playing).store(false, std::memory_order_relaxed);
        std::atomic_ref<unsigned int>(it->second.current_frame).store(0, std::memory_order_relaxed);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

bool AudioEngine::is_sound_playing(const std::string& name) {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        return std::atomic_ref<bool>(it->second.is_playing).load(std::memory_order_relaxed);
    }
    spdlog::warn("Sound {} not found", name);
    return false;
}

void AudioEngine::set_sound_volume(const std::string& name, float volume) {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        std::atomic_ref<float>(it->second.volume).store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

void AudioEngine::set_sound_pan(const std::string& name, float pan) {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        std::atomic_ref<float>(it->second.pan).store(std::clamp(pan, 0.0f, 1.0f), std::memory_order_relaxed);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

void AudioEngine::set_sound_pitch(const std::string& name, float pitch) {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        std::atomic_ref<float>(it->second.pitch).store(pitch, std::memory_order_relaxed);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

float AudioEngine::get_sound_time_played(const std::string& name) const {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        unsigned int frame = std::atomic_ref<unsigned int>(const_cast<unsigned int&>(it->second.current_frame))
                                 .load(std::memory_order_relaxed);
        return static_cast<float>(frame) / static_cast<float>(target_sample_rate);
    }
    spdlog::warn("Sound {} not found", name);
    return 0.0f;
}

void AudioEngine::seek_sound(const std::string& name, float position) {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        sound& snd = it->second;
        unsigned int frame = static_cast<unsigned int>(std::max(position, 0.0f) * static_cast<float>(target_sample_rate));
        std::atomic_ref<unsigned int>(snd.current_frame).store(std::min(frame, snd.frame_count), std::memory_order_relaxed);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

std::string AudioEngine::load_music_stream(const fs::path& file_path, const std::string& name) {
    try {
        SF_INFO file_info;
        std::memset(&file_info, 0, sizeof(SF_INFO));

        SNDFILE* file = nullptr;
        #ifdef _WIN32
        file = sf_wchar_open(file_path.wstring().c_str(), SFM_READ, &file_info);
        #endif

        if (!file) {
            std::string path_str = path_to_string(file_path);
            file = sf_open(path_str.c_str(), SFM_READ, &file_info);
        }

        if (!file) {
#ifdef __ANDROID__
            float* ff_data = nullptr;
            sf_count_t ff_frames = 0;
            unsigned int ff_rate = 0, ff_ch = 0;
            std::string path_str2 = path_to_string(file_path);
            if (!ffmpeg_decode_float(path_str2.c_str(), &ff_data, &ff_frames, &ff_rate, &ff_ch)) {
                spdlog::error("Failed to open music file: {} - {} (ffmpeg fallback also failed)",
                              file_path.string(), sf_strerror(NULL));
                return "";
            }

            if ((double)ff_rate != target_sample_rate) {
                double ratio = target_sample_rate / (double)ff_rate;
                long out_frames = (long)(ff_frames * ratio) + 1;
                float* rs = new float[out_frames * ff_ch];
                SRC_DATA sd{};
                sd.data_in = ff_data; sd.input_frames = ff_frames;
                sd.data_out = rs; sd.output_frames = out_frames;
                sd.src_ratio = ratio; sd.end_of_input = 1;
                int err = src_simple(&sd, SRC_SINC_FASTEST, (int)ff_ch);
                if (err) { delete[] ff_data; delete[] rs; return ""; }
                delete[] ff_data;
                ff_data = rs;
                ff_frames = sd.output_frames_gen;
                ff_rate = (unsigned int)target_sample_rate;
            }

            music mus{};
            mus.file_handle = nullptr;
            mus.file_info.channels = (int)ff_ch;
            mus.file_info.samplerate = (int)ff_rate;
            mus.file_path = file_path.string();
            mus.pcm_data = ff_data;
            mus.pcm_total_frames = ff_frames;
            mus.buffer_size = 4096;
            mus.stream_buffer = new float[mus.buffer_size * ff_ch];
            mus.buffer_position = 0;
            mus.frames_in_buffer = 0;
            mus.is_playing = false;
            mus.current_frame = 0;
            mus.loop = false;
            mus.volume = 1.0f;
            mus.pan = 0.5f;
            mus.pitch = 1.0f;
            mus.resampler = nullptr;
            mus.resample_buffer = nullptr;
            {
                std::unique_lock<std::shared_mutex> guard(rw_lock);
                music_streams[name] = mus;
            }
            spdlog::debug("Loaded music stream (ffmpeg): {} ({} frames, {} Hz, {} ch)",
                          name, ff_frames, ff_rate, ff_ch);
            return name;
#else
            spdlog::error("Failed to open music file: {} - {}", file_path.string(), sf_strerror(NULL));
            return "";
#endif
        }

        music mus;
        mus.file_handle = file;
        mus.file_info = file_info;
        mus.file_path = file_path.string();

        mus.buffer_size = 4096; //arbitrary buffer size?
        mus.stream_buffer = new float[mus.buffer_size * file_info.channels];
        mus.buffer_position = 0;
        mus.frames_in_buffer = 0;

        mus.is_playing = false;
        mus.current_frame = 0;
        mus.loop = false;
        mus.volume = 1.0f;
        mus.pan = 0.5f;
        mus.pitch = 1.0f;

        if (file_info.samplerate != target_sample_rate) {
            int error;
            mus.resampler = src_new(SRC_SINC_FASTEST, file_info.channels, &error);
            if (!mus.resampler) {
                spdlog::error("Failed to create resampler for music stream {}: {}", name, src_strerror(error));
                sf_close(file);
                delete[] mus.stream_buffer;
                return "";
            }

            double ratio = target_sample_rate / (double)file_info.samplerate;
            unsigned int resample_buffer_size = (unsigned int)(mus.buffer_size * ratio) + 256;
            mus.resample_buffer = new float[resample_buffer_size * file_info.channels];

            spdlog::info("Music stream {} will be resampled from {} Hz to {} Hz",
                        name, file_info.samplerate, target_sample_rate);
        } else {
            mus.resampler = nullptr;
            mus.resample_buffer = nullptr;
        }

        {
            std::unique_lock<std::shared_mutex> guard(rw_lock);
            music_streams[name] = mus;
        }

        spdlog::debug("Loaded music stream: {} ({} frames, {} Hz, {} channels)",
                     name, file_info.frames, file_info.samplerate, file_info.channels);

        return name;

    } catch (const std::exception& e) {
        spdlog::error("Error loading music stream {}: {}", file_path.string(), e.what());
        return "";
    }
}

#ifndef __EMSCRIPTEN__
std::string AudioEngine::load_music_stream_memory(
    const av::AVAudioStream& audio_stream,
    const std::string&       name)
{
    try {
        std::vector<uint8_t> encoded = audio_stream.encoded_bytes();
        if (encoded.empty()) {
            spdlog::error("load_music_stream_memory: no audio data for stream '{}'", name);
            return "";
        }

        // Build the struct and move it into the map BEFORE opening the sndfile
        // handle, so that vio_cursor lives at its final stable address when
        // sf_open_virtual stores a pointer to it.
        music mus{};
        mus.memory_buffer    = std::make_shared<std::vector<uint8_t>>(std::move(encoded));
        mus.file_handle      = nullptr;
        mus.file_path        = "<memory:" + name + ">";
        mus.buffer_size      = 4096;
        mus.buffer_position  = 0;
        mus.frames_in_buffer = 0;
        mus.is_playing       = false;
        mus.current_frame    = 0;
        mus.loop             = false;
        mus.volume           = 1.0f;
        mus.pan              = 0.5f;
        mus.pitch            = 1.0f;
        mus.resampler        = nullptr;
        mus.resample_buffer  = nullptr;
        mus.stream_buffer    = nullptr;

        std::unique_lock<std::shared_mutex> guard(rw_lock);
        music_streams[name] = std::move(mus);
        music& stored = music_streams[name];  // reference into the map – stable address

        // vio_cursor now lives inside the map entry and will never move again.
        stored.vio_cursor = VirtualFile{ stored.memory_buffer.get(), 0 };

        SF_VIRTUAL_IO vio{};
        vio.get_filelen = vf_get_filelen;
        vio.seek        = vf_seek;
        vio.read        = vf_read;
        vio.write       = vf_write;
        vio.tell        = vf_tell;

        SF_INFO file_info{};
        std::memset(&file_info, 0, sizeof(SF_INFO));

        stored.file_handle = sf_open_virtual(&vio, SFM_READ, &file_info, &stored.vio_cursor);
        if (!stored.file_handle) {
            spdlog::error("load_music_stream_memory: sf_open_virtual failed for '{}': {}",
                          name, sf_strerror(nullptr));
            music_streams.erase(name);
            return "";
        }

        stored.file_info     = file_info;
        stored.stream_buffer = new float[stored.buffer_size * file_info.channels];

        if (file_info.samplerate != target_sample_rate) {
            int error;
            stored.resampler = src_new(SRC_SINC_FASTEST, file_info.channels, &error);
            if (!stored.resampler) {
                spdlog::error("Failed to create resampler for memory stream '{}': {}",
                              name, src_strerror(error));
                sf_close(stored.file_handle);
                delete[] stored.stream_buffer;
                music_streams.erase(name);
                return "";
            }
            double       ratio            = target_sample_rate / (double)file_info.samplerate;
            unsigned int resample_buf_size = (unsigned int)(stored.buffer_size * ratio) + 256;
            stored.resample_buffer = new float[resample_buf_size * file_info.channels];
            spdlog::info("Memory music stream '{}' will be resampled from {} Hz to {} Hz",
                         name, file_info.samplerate, target_sample_rate);
        }

        spdlog::debug("Loaded memory music stream: '{}' ({} frames, {} Hz, {} channels)",
                      name, file_info.frames, file_info.samplerate, file_info.channels);
        return name;

    } catch (const std::exception& e) {
        spdlog::error("Error loading memory music stream '{}': {}", name, e.what());
        return "";
    }
}
#endif // __EMSCRIPTEN__

void AudioEngine::play_music_stream(const std::string& name, VolumePreset volume_preset) {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        music& mus = it->second;

        if (volume_preset != VolumePreset::NONE) {
            float volume = 1.0f;
            if      (volume_preset == VolumePreset::SOUND)       volume = volume_presets.sound;
            else if (volume_preset == VolumePreset::MUSIC)       volume = volume_presets.music;
            else if (volume_preset == VolumePreset::VOICE)       volume = volume_presets.voice;
            else if (volume_preset == VolumePreset::HITSOUND)    volume = volume_presets.hitsound;
            else if (volume_preset == VolumePreset::ATTRACT_MODE) volume = volume_presets.attract_mode;
            std::atomic_ref<float>(mus.volume).store(volume, std::memory_order_relaxed);
        }

        std::atomic_ref<unsigned long long>(mus.current_frame).store(0, std::memory_order_relaxed);
        std::atomic_ref<bool>(mus.is_playing).store(true, std::memory_order_release);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

float AudioEngine::get_music_time_length(const std::string& name) const {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        if (it->second.pcm_data)
            return static_cast<float>(it->second.pcm_total_frames) / static_cast<float>(target_sample_rate);
        return static_cast<float>(it->second.file_info.frames) / static_cast<float>(target_sample_rate);
    }
    spdlog::warn("Music stream {} not found", name);
    return 0.0f;
}

float AudioEngine::get_music_time_played(const std::string& name) const {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        unsigned long long frame = std::atomic_ref<unsigned long long>(
            const_cast<unsigned long long&>(it->second.current_frame))
            .load(std::memory_order_relaxed);
        return static_cast<float>(frame) / static_cast<float>(target_sample_rate);
    }
    spdlog::warn("Music stream {} not found", name);
    return 0.0f;
}

void AudioEngine::set_music_volume(const std::string& name, float volume) {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        std::atomic_ref<float>(it->second.volume).store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

bool AudioEngine::is_music_stream_valid(const std::string& name) const {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    return music_streams.find(name) != music_streams.end();
}

bool AudioEngine::is_music_stream_playing(const std::string& name) const {
    std::shared_lock<std::shared_mutex> guard(rw_lock);
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        return std::atomic_ref<bool>(const_cast<bool&>(it->second.is_playing))
                   .load(std::memory_order_relaxed);
    }
    spdlog::warn("Sound {} not found", name);
    return false;
}

void AudioEngine::stop_music_stream(const std::string& name) {
    std::unique_lock<std::shared_mutex> guard(rw_lock);
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        music& mus = it->second;
        mus.is_playing      = false;
        mus.current_frame   = 0;
        mus.buffer_position  = 0;
        mus.frames_in_buffer = 0;

        if (mus.file_handle) {
            sf_seek(mus.file_handle, 0, SEEK_SET);
        }
    } else {
        spdlog::warn("Music stream {} not found", name);
    }
}

void AudioEngine::unload_music_stream(const std::string& name) {
    std::unique_lock<std::shared_mutex> guard(rw_lock);
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        music& mus = it->second;

        mus.is_playing = false;

        if (mus.file_handle) {
            sf_close(mus.file_handle);
            mus.file_handle = nullptr;
        }

        if (mus.pcm_data) {
            delete[] mus.pcm_data;
            mus.pcm_data = nullptr;
        }

        if (mus.stream_buffer) {
            delete[] mus.stream_buffer;
            mus.stream_buffer = nullptr;
        }

        if (mus.resampler) {
            src_delete(mus.resampler);
            mus.resampler = nullptr;
        }

        if (mus.resample_buffer) {
            delete[] mus.resample_buffer;
            mus.resample_buffer = nullptr;
        }

        music_streams.erase(it);
        spdlog::debug("Unloaded music stream: {}", name);
    } else {
        spdlog::warn("Music stream {} not found", name);
    }
}

void AudioEngine::unload_all_music() {
    std::vector<std::string> names;
    {
        std::shared_lock<std::shared_mutex> guard(rw_lock);
        names.reserve(music_streams.size());
        for (const auto& [name, _] : music_streams) names.push_back(name);
    }

    for (const auto& name : names) {
        unload_music_stream(name);
    }

    spdlog::info("All music streams unloaded");
}

void AudioEngine::seek_music_stream(const std::string& name, float position) {
    std::unique_lock<std::shared_mutex> guard(rw_lock);
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        music& mus = it->second;

        if (mus.file_handle) {
            sf_count_t frame_position = static_cast<sf_count_t>(position * mus.file_info.samplerate);

            if (frame_position < 0) frame_position = 0;
            if (frame_position >= mus.file_info.frames) frame_position = mus.file_info.frames - 1;

            sf_seek(mus.file_handle, frame_position, SEEK_SET);

            mus.buffer_position  = 0;
            mus.frames_in_buffer = 0;
            mus.current_frame    = frame_position;
        } else if (mus.pcm_data) {
            unsigned long long frame_pos = static_cast<unsigned long long>(
                std::max(position, 0.0f) * static_cast<float>(target_sample_rate));
            if (frame_pos > static_cast<unsigned long long>(mus.pcm_total_frames))
                frame_pos = static_cast<unsigned long long>(mus.pcm_total_frames);
            std::atomic_ref<unsigned long long>(mus.current_frame).store(frame_pos, std::memory_order_relaxed);
            mus.buffer_position  = 0;
            mus.frames_in_buffer = 0;
        }
    } else {
        spdlog::warn("Music stream {} not found", name);
    }
}

AudioEngine audio;
