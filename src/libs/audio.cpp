#ifndef AUDIO_BACKEND_RAYLIB
#include "audio.h"

//for use with load_music_stream_memory

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


AudioEngine::AudioEngine(int host_api_index, float sample_rate, unsigned long buffer_size, const VolumeConfig& volume_presets)
    : host_api_index(std::max(host_api_index, 0))
    , target_sample_rate(sample_rate < 0 ? 44100.0f : sample_rate)
    , buffer_size(buffer_size)
    , volume_presets(volume_presets)
    , is_ready(false)
    , master_volume(1.0f)
    , stream(nullptr)
{
    fs::path skin_path = fs::path("Skins") / global_data.config->paths.skin / "Sounds";
    if (fs::exists(skin_path)) {
        sounds_path = skin_path;
    } else {
        spdlog::error("Skin directory not found, skipping audio initialization");
    }
}

AudioEngine::~AudioEngine() {
    if (is_ready) {
        close_audio_device();
    }
}

int AudioEngine::port_audio_callback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo* timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData) {

    float* out = static_cast<float*>(outputBuffer);

    AudioEngine* engine = static_cast<AudioEngine*>(userData);

    if (!engine) {
        std::memset(out, 0, framesPerBuffer * 2 * sizeof(float));
        return paContinue;
    }

    engine->lock.lock();

    // Initialize output buffer with silence
    const unsigned long buffer_size = framesPerBuffer * 2;
    for (unsigned long i = 0; i < buffer_size; i++) {
        out[i] = 0.0f;
    }

    for (auto& [name, snd] : engine->sounds) {
        if (!snd.is_playing) continue;

        unsigned long frames_to_process = framesPerBuffer;
        unsigned long output_index = 0;

        while (frames_to_process > 0 && snd.is_playing) {
            if (snd.current_frame >= snd.frame_count) {
                if (snd.loop) {
                    snd.current_frame = 0;
                } else {
                    snd.is_playing = false;
                    break;
                }
            }

            unsigned long frames_available = snd.frame_count - snd.current_frame;
            unsigned long frames_to_read = (frames_to_process < frames_available) ? frames_to_process : frames_available;

            const float volume = snd.volume;
            const float pan = snd.pan;
            const float* data_ptr = snd.data;
            const unsigned int channels = snd.channels;

            for (unsigned long i = 0; i < frames_to_read; i++) {
                unsigned long src_index = (snd.current_frame + i) * channels;
                unsigned long dst_index = (output_index + i) * 2;  // Stereo output

                float left, right;

                if (channels == 1) {
                    // Mono source - apply pan
                    float sample = data_ptr[src_index];
                    left = sample * (1.0f - pan);
                    right = sample * pan;
                } else {
                    // Stereo source - apply pan as balance
                    left = data_ptr[src_index];
                    right = data_ptr[src_index + 1];

                    if (pan < 0.5f) {
                        right *= (pan * 2.0f);
                    } else if (pan > 0.5f) {
                        left *= ((1.0f - pan) * 2.0f);
                    }
                }

                out[dst_index] += left * volume;
                out[dst_index + 1] += right * volume;
            }

            snd.current_frame += frames_to_read;
            output_index += frames_to_read;
            frames_to_process -= frames_to_read;
        }
    }

    for (auto& [name, mus] : engine->music_streams) {
        if (!mus.is_playing) continue;

        unsigned long frames_to_process = framesPerBuffer;
        unsigned long output_index = 0;

        while (frames_to_process > 0 && mus.is_playing) {
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
                            mus.is_playing = false;
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
                            mus.is_playing = false;
                            break;
                        }

                        mus.frames_in_buffer = src_data.output_frames_gen;
                    } else {
                        mus.frames_in_buffer = frames_read;
                    }

                    mus.buffer_position = 0;
                } else {
                    mus.is_playing = false;
                    break;
                }
            }

            unsigned long frames_available = mus.frames_in_buffer - mus.buffer_position;
            unsigned long frames_to_read = (frames_to_process < frames_available) ? frames_to_process : frames_available;

            const unsigned int channels = mus.file_info.channels;
            const float* source_buffer = mus.resampler ? mus.resample_buffer : mus.stream_buffer;
            const float volume = mus.volume;
            const float pan = mus.pan;

            for (unsigned long i = 0; i < frames_to_read; i++) {
                unsigned long src_index = (mus.buffer_position + i) * channels;
                unsigned long dst_index = (output_index + i) * 2;  // Stereo output

                float left, right;

                if (channels == 1) {
                    // Mono source - apply pan
                    float sample = source_buffer[src_index];
                    left = sample * (1.0f - pan);
                    right = sample * pan;
                } else {
                    // Stereo source - apply pan as balance
                    left = source_buffer[src_index];
                    right = source_buffer[src_index + 1];

                    if (pan < 0.5f) {
                        right *= (pan * 2.0f);
                    } else if (pan > 0.5f) {
                        left *= ((1.0f - pan) * 2.0f);
                    }
                }

                out[dst_index] += left * volume;
                out[dst_index + 1] += right * volume;
            }

            mus.buffer_position += frames_to_read;
            mus.current_frame += frames_to_read;
            output_index += frames_to_read;
            frames_to_process -= frames_to_read;
        }
    }

    const float master_vol = engine->master_volume;
    const unsigned long output_buffer_size = framesPerBuffer * 2;

    for (unsigned long i = 0; i < output_buffer_size; i++) {
        float sample = out[i] * master_vol;
        out[i] = (sample > 1.0f) ? 1.0f : ((sample < -1.0f) ? -1.0f : sample);
    }

    engine->lock.unlock();

    return paContinue;
}


bool AudioEngine::init_audio_device() {
    try {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            spdlog::error("Failed to initialize PortAudio: {}", Pa_GetErrorText(err));
            return false;
        }

        const PaHostApiInfo *host_api_info = Pa_GetHostApiInfo(host_api_index);
        if (!host_api_info) {
            spdlog::error("Failed to get host API info for index {}", host_api_index);
            Pa_Terminate();
            return false;
        }

        PaDeviceIndex device_index = host_api_info->defaultOutputDevice;
        if (device_index == paNoDevice) {
            spdlog::error("No default output device found for host API {}", host_api_info->name);
            Pa_Terminate();
            return false;
        }

        const PaDeviceInfo *device_info = Pa_GetDeviceInfo(device_index);
        if (!device_info) {
            spdlog::error("Failed to get device info for device {}", device_index);
            Pa_Terminate();
            return false;
        }

        output_parameters.device = device_index;
        output_parameters.channelCount = std::min(2, device_info->maxOutputChannels);
        output_parameters.sampleFormat = paFloat32;
        output_parameters.suggestedLatency = device_info->defaultLowOutputLatency;
        output_parameters.hostApiSpecificStreamInfo = NULL;


        err = Pa_OpenStream(&stream, NULL, &output_parameters, target_sample_rate, buffer_size, paNoFlag, port_audio_callback, this);
        if (err != paNoError) {
            spdlog::error("Failed to open audio stream: {}", Pa_GetErrorText(err));
            Pa_Terminate();
            return false;
        }

        err = Pa_StartStream(stream);
        if (err != paNoError) {
            spdlog::error("Failed to start audio stream: {}", Pa_GetErrorText(err));
            Pa_CloseStream(stream);
            Pa_Terminate();
            stream = nullptr;
            return false;
        }

        is_ready = true;

        spdlog::info("Audio Device initialized successfully");
        spdlog::info("    > Backend:       PortAudio | {}", host_api_info->name);
        spdlog::info("    > Device:        {}", device_info->name);
        spdlog::info("    > Format:        {}", "Float32");
        const PaStreamInfo *streamInfo = Pa_GetStreamInfo(stream);
        spdlog::info("    > Channels:      {}", output_parameters.channelCount);
        spdlog::info("    > Sample rate:   {}", target_sample_rate);
        spdlog::info("    > Buffer size:   {} (requested)", buffer_size);
        spdlog::info("    > Latency:       {} ms", streamInfo->outputLatency * 1000.0);

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

        if (stream != nullptr) {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            stream = nullptr;
        }
        Pa_Terminate();
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
    lock.lock();
    master_volume = std::clamp(volume, 0.0f, 1.0f);
    lock.unlock();
}

float AudioEngine::get_master_volume() {
    lock.lock();
    float volume = master_volume;
    lock.unlock();
    return volume;
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
        std::string path_str = path_to_string(file_path);

        SF_INFO file_info;
        std::memset(&file_info, 0, sizeof(SF_INFO));

        SNDFILE* file = sf_open(path_str.c_str(), SFM_READ, &file_info);

        if (!file) {
            // Try UTF-8 encoding as fallback
            path_str = file_path.string();
            file = sf_open(path_str.c_str(), SFM_READ, &file_info);
        }

        if (!file) {
            spdlog::error("Failed to open sound file: {} - {}", file_path.string(), sf_strerror(NULL));
            return "";
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

        lock.lock();
        sounds[name] = snd;
        lock.unlock();

        spdlog::debug("Loaded sound: {} ({} frames, {} Hz, {} channels)",
                     name, snd.frame_count, snd.sample_rate, snd.channels);

        return name;

    } catch (const std::exception& e) {
        spdlog::error("Error loading sound {}: {}", file_path.string(), e.what());
        return "";
    }
}

void AudioEngine::unload_sound(const std::string& name) {
    lock.lock();
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        sound& snd = it->second;

        snd.is_playing = false;

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
    lock.unlock();
}

void AudioEngine::unload_all_sounds() {
    lock.lock();
    auto sound_names = sounds;
    lock.unlock();

    for (const auto& [name, _] : sound_names) {
        unload_sound(name);
    }

    spdlog::info("All sounds unloaded");
}

void AudioEngine::play_sound(const std::string& name, const std::string& volume_preset) {
    lock.lock();
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        sound& snd = it->second;

        if (!volume_preset.empty()) {
            float volume = 1.0f;
            if (volume_preset == "sound") volume = volume_presets.sound;
            else if (volume_preset == "music") volume = volume_presets.music;
            else if (volume_preset == "voice") volume = volume_presets.voice;
            else if (volume_preset == "hitsound") volume = volume_presets.hitsound;
            else if (volume_preset == "attract_mode") volume = volume_presets.attract_mode;
            snd.volume = volume;
        }

        snd.current_frame = 0;
        snd.is_playing = true;
    } else {
        spdlog::warn("Sound {} not found", name);
    }
    lock.unlock();
}

void AudioEngine::stop_sound(const std::string& name) {
    lock.lock();
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        it->second.is_playing = false;
        it->second.current_frame = 0;
    } else {
        spdlog::warn("Sound {} not found", name);
    }
    lock.unlock();
}

bool AudioEngine::is_sound_playing(const std::string& name) {
    lock.lock();
    auto it = sounds.find(name);
    bool playing = false;
    if (it != sounds.end()) {
        playing = it->second.is_playing;
    } else {
        spdlog::warn("Sound {} not found", name);
    }
    lock.unlock();
    return playing;
}

void AudioEngine::set_sound_volume(const std::string& name, float volume) {
    lock.lock();
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        it->second.volume = std::clamp(volume, 0.0f, 1.0f);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
    lock.unlock();
}

void AudioEngine::set_sound_pan(const std::string& name, float pan) {
    lock.lock();
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        it->second.pan = std::clamp(pan, 0.0f, 1.0f);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
    lock.unlock();
}

float AudioEngine::get_sound_time_played(const std::string& name) const {
    lock.lock();
    auto it = sounds.find(name);
    float time = 0.0f;
    if (it != sounds.end()) {
        time = static_cast<float>(it->second.current_frame) / static_cast<float>(target_sample_rate);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
    lock.unlock();
    return time;
}

void AudioEngine::seek_sound(const std::string& name, float position) {
    lock.lock();
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        sound& snd = it->second;
        unsigned int frame = static_cast<unsigned int>(std::max(position, 0.0f) * static_cast<float>(target_sample_rate));
        snd.current_frame = std::min(frame, snd.frame_count);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
    lock.unlock();
}

std::string AudioEngine::load_music_stream(const fs::path& file_path, const std::string& name) {
    try {
        std::string path_str = path_to_string(file_path);

        SF_INFO file_info;
        std::memset(&file_info, 0, sizeof(SF_INFO));

        SNDFILE* file = sf_open(path_str.c_str(), SFM_READ, &file_info);

        if (!file) {
            path_str = file_path.string();
            file = sf_open(path_str.c_str(), SFM_READ, &file_info);
        }

        #ifdef _WIN32
        if (!file) {
            file = sf_wchar_open(path_str.c_str(), SFM_READ, &file_info);
        }
        #endif

        if (!file) {
            spdlog::error("Failed to open music file: {} - {}", file_path.string(), sf_strerror(NULL));
            return "";
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

        lock.lock();
        music_streams[name] = mus;
        lock.unlock();

        spdlog::debug("Loaded music stream: {} ({} frames, {} Hz, {} channels)",
                     name, file_info.frames, file_info.samplerate, file_info.channels);

        return name;

    } catch (const std::exception& e) {
        spdlog::error("Error loading music stream {}: {}", file_path.string(), e.what());
        return "";
    }
}

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

        lock.lock();
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
            lock.unlock();
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
                lock.unlock();
                return "";
            }
            double       ratio            = target_sample_rate / (double)file_info.samplerate;
            unsigned int resample_buf_size = (unsigned int)(stored.buffer_size * ratio) + 256;
            stored.resample_buffer = new float[resample_buf_size * file_info.channels];
            spdlog::info("Memory music stream '{}' will be resampled from {} Hz to {} Hz",
                         name, file_info.samplerate, target_sample_rate);
        }

        lock.unlock();

        spdlog::debug("Loaded memory music stream: '{}' ({} frames, {} Hz, {} channels)",
                      name, file_info.frames, file_info.samplerate, file_info.channels);
        return name;

    } catch (const std::exception& e) {
        spdlog::error("Error loading memory music stream '{}': {}", name, e.what());
        return "";
    }
}

void AudioEngine::play_music_stream(const std::string& name, const std::string& volume_preset) {
    lock.lock();
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        music& mus = it->second;

        if (!volume_preset.empty()) {
            float volume = 1.0f;
            if (volume_preset == "sound") volume = volume_presets.sound;
            else if (volume_preset == "music") volume = volume_presets.music;
            else if (volume_preset == "voice") volume = volume_presets.voice;
            else if (volume_preset == "hitsound") volume = volume_presets.hitsound;
            else if (volume_preset == "attract_mode") volume = volume_presets.attract_mode;
            mus.volume = volume;
        }

        mus.current_frame = 0;
        mus.is_playing = true;
    } else {
        spdlog::warn("Sound {} not found", name);
    }
    lock.unlock();
}

float AudioEngine::get_music_time_length(const std::string& name) const {
    lock.lock();
    auto it = music_streams.find(name);
    float length = 0.0f;
    if (it != music_streams.end()) {
        const music& mus = it->second;
        length = static_cast<float>(mus.file_info.frames) / static_cast<float>(target_sample_rate);
    } else {
        spdlog::warn("Music stream {} not found", name);
    }
    lock.unlock();
    return length;
}

float AudioEngine::get_music_time_played(const std::string& name) const {
    lock.lock();
    auto it = music_streams.find(name);
    float time = 0.0f;
    if (it != music_streams.end()) {
        const music& mus = it->second;
        time = static_cast<float>(mus.current_frame) / static_cast<float>(target_sample_rate);
    } else {
        spdlog::warn("Music stream {} not found", name);
    }
    lock.unlock();
    return time;
}

void AudioEngine::set_music_volume(const std::string& name, float volume) {
    lock.lock();
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        it->second.volume = std::clamp(volume, 0.0f, 1.0f);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
    lock.unlock();
}

bool AudioEngine::is_music_stream_valid(const std::string& name) const {
    lock.lock();
    bool valid = music_streams.find(name) != music_streams.end();
    lock.unlock();
    return valid;
}

bool AudioEngine::is_music_stream_playing(const std::string& name) const {
    lock.lock();
    auto it = music_streams.find(name);
    bool playing = false;
    if (it != music_streams.end()) {
        playing = it->second.is_playing;
    } else {
        spdlog::warn("Sound {} not found", name);
    }
    lock.unlock();
    return playing;
}

void AudioEngine::stop_music_stream(const std::string& name) {
    lock.lock();
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        it->second.is_playing = false;
        it->second.current_frame = 0;
        it->second.buffer_position = 0;
        it->second.frames_in_buffer = 0;

        if (it->second.file_handle) {
            sf_seek(it->second.file_handle, 0, SEEK_SET);
        }
    } else {
        spdlog::warn("Music stream {} not found", name);
    }
    lock.unlock();
}

void AudioEngine::unload_music_stream(const std::string& name) {
    lock.lock();
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        music& mus = it->second;

        mus.is_playing = false;

        if (mus.file_handle) {
            sf_close(mus.file_handle);
            mus.file_handle = nullptr;
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
    lock.unlock();
}

void AudioEngine::unload_all_music() {
    lock.lock();
    auto music_names = music_streams;
    lock.unlock();

    for (const auto& [name, _] : music_names) {
        unload_music_stream(name);
    }

    spdlog::info("All music streams unloaded");
}

void AudioEngine::seek_music_stream(const std::string& name, float position) {
    lock.lock();
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        music& mus = it->second;

        if (mus.file_handle) {
            sf_count_t frame_position = static_cast<sf_count_t>(position * mus.file_info.samplerate);

            if (frame_position < 0) frame_position = 0;
            if (frame_position >= mus.file_info.frames) frame_position = mus.file_info.frames - 1;

            sf_seek(mus.file_handle, frame_position, SEEK_SET);

            mus.buffer_position = 0;
            mus.frames_in_buffer = 0;
            mus.current_frame = frame_position;
        }
    } else {
        spdlog::warn("Music stream {} not found", name);
    }
    lock.unlock();
}

#endif  // AUDIO_BACKEND_RAYLIB
