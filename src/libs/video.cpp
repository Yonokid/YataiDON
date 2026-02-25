#include "video.h"

VideoPlayer::VideoPlayer(fs::path path)
    : is_finished_arr{false, false}
{
    if (path.extension() == ".png" || path.extension() == ".jpg") {
        texture  = ray::LoadTexture(path.c_str());
        is_static = true;
        return;
    }

    container    = av::AVContainer::open(path);
    video_stream = container->streams().video(0);
    audio_stream = container->streams().audio(0);

    audio_s = audio->load_music_stream_memory(*audio_stream, "video_player");

    fps = video_stream->average_rate().value_or(0.f);

    duration = (container->duration() > 0)
                   ? static_cast<double>(container->duration()) / av::AVContainer::time_base()
                   : 0.0;

    width  = static_cast<float>(video_stream->width());
    height = static_cast<float>(video_stream->height());

    frame_count = (fps > 0.f) ? static_cast<int>(duration * fps) + 1 : 0;
    frame_timestamps.reserve(frame_count);
    for (int i = 0; i < frame_count; ++i) {
        frame_timestamps.push_back((i * 1000.0) / fps);
    }

    frame_index    = 0;
    frame_duration = (fps > 0.f) ? 1000.0 / fps : 0.0;
}

VideoPlayer::~VideoPlayer() {
    stop();
}

void VideoPlayer::audio_manager() {
    if (is_finished_arr[1]) return;

    if (!audio_started) {
        audio->play_music_stream(audio_s);
        audio_started = true;
    }

    is_finished_arr[1] =
        audio->get_music_time_played(audio_s) >= audio->get_music_time_length(audio_s);
}

void VideoPlayer::init_frame_generator() {
    container->seek(0);
    frame_generator = container->decode_video(0);
}

bool VideoPlayer::get_next_frame_bytes(
    std::vector<uint8_t>& out_bytes,
    int& out_width,
    int& out_height)
{
    if (!frame_generator) {
        init_frame_generator();
    }

    if (!frame_generator->next(current_decoded_frame)) {
        return false;
    }

    current_decoded_frame->reformat("rgb24");

    out_width  = current_decoded_frame->width();
    out_height = current_decoded_frame->height();

    const uint8_t* data = current_decoded_frame->plane(0).data();
    std::size_t    size = current_decoded_frame->plane(0).size();
    out_bytes.assign(data, data + size);

    return true;
}

bool VideoPlayer::load_frame(int index) {
    if (index < 0 || index >= static_cast<int>(frame_timestamps.size())) {
        return false;
    }

    std::vector<uint8_t> frame_bytes;
    int frame_w = 0, frame_h = 0;

    if (!get_next_frame_bytes(frame_bytes, frame_w, frame_h)) {
        return false;
    }

    const void* pixels_ptr = static_cast<const void*>(frame_bytes.data());

    if (!texture.has_value()) {
        ray::Image image{};
        image.data    = const_cast<void*>(pixels_ptr);   // raylib copies on load
        image.width   = frame_w;
        image.height  = frame_h;
        image.mipmaps = 1;
        image.format  = ray::PIXELFORMAT_UNCOMPRESSED_R8G8B8;

        ray::Texture2D tex = ray::LoadTextureFromImage(image);
        ray::SetTextureFilter(tex, ray::TEXTURE_FILTER_TRILINEAR);
        texture = tex;
    } else {
        ray::UpdateTexture(texture.value(), pixels_ptr);
    }

    current_frame_data = std::move(frame_bytes);
    return true;
}

bool VideoPlayer::is_started() const {
    return start_ms.has_value();
}

void VideoPlayer::start(double current_ms) {
    if (is_static) return;

    start_ms = current_ms;
    init_frame_generator();
    load_frame(0);
}

bool VideoPlayer::is_finished() const {
    return is_finished_arr[0] && is_finished_arr[1];
}

void VideoPlayer::set_volume(float volume) {
    if (is_static) return;
    audio->set_music_volume(audio_s, volume);
}

void VideoPlayer::update(double current_ms) {
    if (is_static) return;

    audio_manager();

    if (frame_index >= static_cast<int>(frame_timestamps.size())) {
        is_finished_arr[0] = true;
        return;
    }

    if (!is_started()) return;

    double elapsed_ms = current_ms - start_ms.value();

    int target_frame = 0;
    for (int i = 0; i < static_cast<int>(frame_timestamps.size()); ++i) {
        if (elapsed_ms >= frame_timestamps[i]) {
            target_frame = i;
        } else {
            break;
        }
    }

    while (frame_index <= target_frame &&
           frame_index < static_cast<int>(frame_timestamps.size()))
    {
        load_frame(frame_index);
        ++frame_index;
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

    audio->stop_music_stream(audio_s);
    audio->unload_music_stream(audio_s);
}
