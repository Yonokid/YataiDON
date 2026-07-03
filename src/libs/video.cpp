#include "video.h"
#include "audio.h"
#include "texture.h"
#include <spdlog/spdlog.h>

VideoPlayer::VideoPlayer(fs::path path)
    : is_finished_arr{false, false}
{
    if (path.extension() == ".png" || path.extension() == ".jpg") {
        texture  = ray::LoadTexture(path.string().c_str());
        if (!ray::IsTextureValid(texture.value())) {
            spdlog::error("Failed to load static texture for video: {}", path.stem().string());
            return;
        }
        is_static = true;
        return;
    }

#ifndef __EMSCRIPTEN__
    container    = av::AVContainer::open(path);
    if (!container) {
        spdlog::error("Failed to open video: {}", path.string());
        return;
    }
    video_stream = container->streams().video(0);
    audio_stream = container->streams().audio(0);

    audio_s = audio.load_music_stream_memory(*audio_stream, "video_player");

    fps = video_stream->average_rate().value_or(0.f);

    duration = (container->duration() > 0)
                   ? static_cast<double>(container->duration()) / av::AVContainer::time_base()
                   : 0.0;

    width  = static_cast<float>(video_stream->width());
    height = static_cast<float>(video_stream->height());

    frame_count = (fps > 0.f) ? static_cast<int>(duration * fps) + 1 : 0;

    frame_index    = 0;
    frame_duration = (fps > 0.f) ? 1000.0 / fps : 0.0;
#endif
}

VideoPlayer::~VideoPlayer() {
    stop();
}

void VideoPlayer::audio_manager() {
    if (is_finished_arr[1]) return;

    if (!audio_started) {
        audio.play_music_stream(audio_s);
        audio_started = true;
    }

    is_finished_arr[1] =
        audio.get_music_time_played(audio_s) >= audio.get_music_time_length(audio_s);
}

void VideoPlayer::decode_loop() {
#ifndef __EMSCRIPTEN__
    try {
        container->seek(0);
        frame_generator = container->decode_video(0);

        int produced = 0;
        while (!decode_stop.load(std::memory_order_relaxed)) {
            if (!frame_generator->next(current_decoded_frame)) break; // EOF
            current_decoded_frame->reformat("rgb24");

            DecodedFrame frame;
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (!spare_buffers.empty()) {
                    frame.bytes = std::move(spare_buffers.back());
                    spare_buffers.pop_back();
                }
            }
            av::AVPlane plane = current_decoded_frame->plane(0);
            frame.bytes.assign(plane.data(), plane.data() + plane.size());
            frame.width  = current_decoded_frame->width();
            frame.height = current_decoded_frame->height();
            frame.index  = produced++;
            current_decoded_frame.reset();

            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [&] {
                return decode_stop.load(std::memory_order_relaxed) ||
                       frame_queue.size() < MAX_QUEUED_FRAMES;
            });
            if (decode_stop.load(std::memory_order_relaxed)) break;
            frame_queue.push_back(std::move(frame));
        }
    } catch (const std::exception& e) {
        spdlog::error("Video decode thread error: {}", e.what());
    }
    std::lock_guard<std::mutex> lock(queue_mutex);
    decode_eof = true;
#endif
}

void VideoPlayer::stop_decode_thread() {
    decode_stop.store(true);
    queue_cv.notify_all();
    if (decode_thread.joinable()) {
        decode_thread.join();
    }
    // Reset the decoder before the container is closed so AVFrameDecoder's
    // fmt_ctx_ pointer is never left dangling
    frame_generator.reset();
    current_decoded_frame.reset();
    std::lock_guard<std::mutex> lock(queue_mutex);
    frame_queue.clear();
    spare_buffers.clear();
    decode_eof = false;
}

void VideoPlayer::upload_frame(const DecodedFrame& frame) {
    const void* pixels_ptr = static_cast<const void*>(frame.bytes.data());

    if (!texture.has_value()) {
        ray::Image image{};
        image.data    = const_cast<void*>(pixels_ptr);   // raylib copies on load
        image.width   = frame.width;
        image.height  = frame.height;
        image.mipmaps = 1;
        image.format  = ray::PIXELFORMAT_UNCOMPRESSED_R8G8B8;

        ray::Texture2D tex = ray::LoadTextureFromImage(image);
        ray::SetTextureFilter(tex, ray::TEXTURE_FILTER_TRILINEAR);
        texture = tex;
    } else {
        ray::UpdateTexture(texture.value(), pixels_ptr);
    }
}

bool VideoPlayer::is_started() const {
    return start_ms.has_value();
}

void VideoPlayer::start(double current_ms) {
    if (is_static || !container) return;

    stop_decode_thread(); // no-op on first start; resets state on restart
    decode_stop.store(false);
    frame_index = 0;
    start_ms = current_ms;
    decode_thread = std::thread(&VideoPlayer::decode_loop, this);
}

bool VideoPlayer::is_finished() const {
    return is_finished_arr[0] && is_finished_arr[1];
}

void VideoPlayer::set_volume(float volume) {
    if (is_static || !container) return;
    audio.set_music_volume(audio_s, volume);
}

void VideoPlayer::update(double current_ms) {
    if (is_static || !container) return;

    audio_manager();

    if (frame_index >= frame_count) {
        is_finished_arr[0] = true;
        return;
    }

    if (!is_started()) return;

    double elapsed_ms = current_ms - start_ms.value();
    int target_frame = (frame_duration > 0.0)
        ? static_cast<int>(elapsed_ms / frame_duration)
        : 0;

    // Drain all due frames from the decode queue but upload only the newest;
    // intermediate catch-up frames skip the GPU entirely
    std::optional<DecodedFrame> latest;
    bool drained_at_eof;
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!frame_queue.empty() && frame_queue.front().index <= target_frame) {
            if (latest.has_value()) {
                spare_buffers.push_back(std::move(latest->bytes));
            }
            latest = std::move(frame_queue.front());
            frame_queue.pop_front();
        }
        drained_at_eof = decode_eof && frame_queue.empty();
    }
    queue_cv.notify_one();

    if (latest.has_value()) {
        upload_frame(latest.value());
        frame_index = latest->index + 1;
        std::lock_guard<std::mutex> lock(queue_mutex);
        spare_buffers.push_back(std::move(latest->bytes));
    } else if (drained_at_eof) {
        is_finished_arr[0] = true;
    }
}

void VideoPlayer::draw() {
    ray::ClearBackground(ray::BLACK);
    if (!texture.has_value()) return;

    ray::Rectangle source{
        0.f, 0.f,
        static_cast<float>(texture->width),
        static_cast<float>(texture->height)
    };

    float texture_aspect = static_cast<float>(texture->width)  / static_cast<float>(texture->height);
    float screen_aspect  = static_cast<float>(tex.screen_width) / static_cast<float>(tex.screen_height);

    float dest_width, dest_height, dest_x, dest_y;

    if (texture_aspect > screen_aspect) {
        //Letterbox
        dest_width  = static_cast<float>(tex.screen_width);
        dest_height = static_cast<float>(tex.screen_width) / texture_aspect;
        dest_x      = 0.f;
        dest_y      = (static_cast<float>(tex.screen_height) - dest_height) / 2.f;
    } else {
        //Pillarbox
        dest_height = static_cast<float>(tex.screen_height);
        dest_width  = static_cast<float>(tex.screen_height) * texture_aspect;
        dest_x      = (static_cast<float>(tex.screen_width) - dest_width) / 2.f;
        dest_y      = 0.f;
    }

    ray::Rectangle destination{ dest_x, dest_y, dest_width, dest_height };
    ray::Vector2   origin{ 0.f, 0.f };

    ray::DrawTexturePro(texture.value(), source, destination, origin, 0.f, ray::WHITE);
}

void VideoPlayer::stop() {
    stop_decode_thread();
    start_ms.reset();

    if (is_static) {
        if (texture.has_value()) {
            ray::UnloadTexture(texture.value());
            texture.reset();
        }
        return;
    }

    if (container) {
        container->close();
    }

    if (texture.has_value()) {
        ray::UnloadTexture(texture.value());
        texture.reset();
    }

    audio.stop_music_stream(audio_s);
    audio.unload_music_stream(audio_s);
}
