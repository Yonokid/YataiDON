#include "audio.h"

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
    std::memset(out, 0, framesPerBuffer * 2 * sizeof(float));

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

            for (unsigned long i = 0; i < frames_to_read; i++) {
                unsigned long src_index = (snd.current_frame + i) * snd.channels;
                unsigned long dst_index = (output_index + i) * 2;  // Stereo output

                float left, right;

                if (snd.channels == 1) {
                    // Mono source - apply pan
                    float sample = snd.data[src_index];
                    left = sample * (1.0f - snd.pan);
                    right = sample * snd.pan;
                } else {
                    // Stereo source - apply pan as balance
                    left = snd.data[src_index];
                    right = snd.data[src_index + 1];

                    if (snd.pan < 0.5f) {
                        right *= (snd.pan * 2.0f);
                    } else if (snd.pan > 0.5f) {
                        left *= ((1.0f - snd.pan) * 2.0f);
                    }
                }

                out[dst_index] += left * snd.volume;
                out[dst_index + 1] += right * snd.volume;
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

            unsigned int channels = mus.file_info.channels;
            float* source_buffer = mus.resampler ? mus.resample_buffer : mus.stream_buffer;

            for (unsigned long i = 0; i < frames_to_read; i++) {
                unsigned long src_index = (mus.buffer_position + i) * channels;
                unsigned long dst_index = (output_index + i) * 2;  // Stereo output

                float left, right;

                if (channels == 1) {
                    // Mono source - apply pan
                    float sample = source_buffer[src_index];
                    left = sample * (1.0f - mus.pan);
                    right = sample * mus.pan;
                } else {
                    // Stereo source - apply pan as balance
                    left = source_buffer[src_index];
                    right = source_buffer[src_index + 1];

                    if (mus.pan < 0.5f) {
                        right *= (mus.pan * 2.0f);
                    } else if (mus.pan > 0.5f) {
                        left *= ((1.0f - mus.pan) * 2.0f);
                    }
                }

                out[dst_index] += left * mus.volume;
                out[dst_index + 1] += right * mus.volume;
            }

            mus.buffer_position += frames_to_read;
            mus.current_frame += frames_to_read;
            output_index += frames_to_read;
            frames_to_process -= frames_to_read;
        }
    }

    float master_vol = engine->master_volume;
    for (unsigned long i = 0; i < framesPerBuffer * 2; i++) {
        out[i] *= master_vol;

        if (out[i] > 1.0f) out[i] = 1.0f;
        if (out[i] < -1.0f) out[i] = -1.0f;
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

        std::string don_path = path_to_string(sounds_path / "don.wav");
        don = load_sound(don_path.c_str(), "sound");

        std::string kat_path = path_to_string(sounds_path / "ka.wav");
        kat = load_sound(kat_path.c_str(), "sound");

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

        spdlog::info("Loaded sound: {} ({} frames, {} Hz, {} channels)",
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

void AudioEngine::load_screen_sounds(const std::string& screen_name) {
    fs::path path = sounds_path / screen_name;
    if (!fs::exists(path)) {
        spdlog::warn("Sounds for screen {} not found", screen_name);
        return;
    }

    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.is_directory()) {
            for (const auto& file : fs::directory_iterator(entry.path())) {
                load_sound(file.path(), entry.path().stem().string() + "_" + file.path().stem().string());
            }
        } else if (entry.is_regular_file()) {
            load_sound(entry.path(), entry.path().stem().string());
        }
    }

    fs::path global_path = sounds_path / "global";
    if (fs::exists(global_path)) {
        for (const auto& entry : fs::directory_iterator(global_path)) {
            if (entry.is_directory()) {
                for (const auto& file : fs::directory_iterator(entry.path())) {
                    load_sound(file.path(), entry.path().stem().string() + "_" + file.path().stem().string());
                }
            } else if (entry.is_regular_file()) {
                load_sound(entry.path(), entry.path().stem().string());
            }
        }
    }
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

std::string AudioEngine::load_music_stream(const fs::path& file_path, const std::string& name) {
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

        spdlog::info("Loaded music stream: {} ({} frames, {} Hz, {} channels)",
                     name, file_info.frames, file_info.samplerate, file_info.channels);

        return name;

    } catch (const std::exception& e) {
        spdlog::error("Error loading music stream {}: {}", file_path.string(), e.what());
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
        length = static_cast<float>(mus.file_info.frames) / static_cast<float>(mus.file_info.samplerate);
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
        time = static_cast<float>(mus.current_frame) / static_cast<float>(mus.file_info.samplerate);
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
        spdlog::info("Unloaded music stream: {}", name);
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

std::unique_ptr<AudioEngine> audio;
