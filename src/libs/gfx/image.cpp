// image code (stb_image) ported from raylib 6.2's rtextures.c

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <spdlog/spdlog.h>
#include <vector>

#include "../../platform/platform.h"
#include "../gfx.h"

// Static copies: the raylib backend links its own non-static stb_image.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#include <stb_image.h>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8 // screenshot names carry song titles
#include <stb_image_write.h>

const char* text_format(const char* fmt, ...) {
    static char buffers[4][1024];
    static int index = 0;
    char* out = buffers[index];
    index = (index + 1) % 4;
    va_list args;
    va_start(args, fmt);
    vsnprintf(out, sizeof(buffers[0]), fmt, args);
    va_end(args);
    return out;
}

std::vector<unsigned char> load_file_bytes(const char* path) {
    std::vector<unsigned char> data;
    if (platform_read_file(path, data)) {
        return data;
    }
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        spdlog::warn("FILEIO: [{}] Failed to open file", path);
        return data;
    }
    std::fseek(f, 0, SEEK_END);
    data.resize(std::max(0L, std::ftell(f)));
    std::fseek(f, 0, SEEK_SET);
    data.resize(std::fread(data.data(), 1, data.size(), f));
    std::fclose(f);
    return data;
}

int get_pixel_data_size(int width, int height, int format) {
    if (width <= 0 || height <= 0) {
        return 0;
    }
    int bpp = 0;
    switch (format) {
    case pixel_format::GRAYSCALE:
        bpp = 1;
        break;
    case pixel_format::GRAY_ALPHA:
        bpp = 2;
        break;
    case pixel_format::RGB8:
        bpp = 3;
        break;
    case pixel_format::RGBA8:
        bpp = 4;
        break;
    default:
        return 0; // only the formats load_image/gen_image_color produce
    }

    if (width > std::numeric_limits<int>::max() / bpp / height) {
        return 0;
    }

    return width * height * bpp;
}

Image load_image(const char* path) {
    std::vector<unsigned char> file = load_file_bytes(path);
    Image image = load_image_from_memory(file);
    if (!image.data && !file.empty()) {
        spdlog::warn("IMAGE: [{}] Failed to load data: {}", path, stbi_failure_reason());
    }
    return image;
}

Image load_image_from_memory(std::span<const unsigned char> data) {
    Image image;
    int comp = 0;
    image.data =
        stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &image.width, &image.height, &comp, 0);
    if (!image.data) {
        return Image{};
    }
    static constexpr int formats[] = {0, pixel_format::GRAYSCALE, pixel_format::GRAY_ALPHA, pixel_format::RGB8,
                                      pixel_format::RGBA8};
    image.mipmaps = 1;
    image.format = formats[comp];
    return image;
}

void unload_image(Image image) {
    std::free(image.data);
}

bool export_image(Image image, const char* path) {
    const int comp = get_pixel_data_size(1, 1, image.format);
    if (!image.data || comp <= 0 || image.width <= 0 || image.height <= 0 ||
        image.width > (std::numeric_limits<int>::max() / image.height - 1) / comp) {
        return false;
    }

    const int stride = image.width * comp;
    if (stride < comp) {
        return false;
    }

    return stbi_write_png(path, image.width, image.height, comp, image.data, stride);
}

Image gen_image_color(int w, int h, Color color) {
    const int size = get_pixel_data_size(w, h, pixel_format::RGBA8);
    if (size <= 0) {
        return {};
    }

    Image image{std::calloc(size, 1), w, h, 1, pixel_format::RGBA8};
    auto* px = static_cast<Color*>(image.data);
    for (int i = 0; px && i < w * h; i++) {
        px[i] = color;
    }
    return image;
}

Image image_copy(Image image) { // CPU images are always a single level
    const int size = get_pixel_data_size(image.width, image.height, image.format);
    Image out = image;
    out.data = image.data && size > 0 ? std::malloc(size) : nullptr;
    if (out.data) {
        std::memcpy(out.data, image.data, size);
    }
    return out;
}

Image image_from_image(Image image, Rect rec) {
    Image out;
    if (!image.data || get_pixel_data_size(image.width, image.height, image.format) <= 0) {
        return out;
    }
    if (rec.width <= 0 || rec.height <= 0) {
        return out; // empty (e.g. a space glyph)
    }
    if (rec.x < 0 || rec.y < 0 || rec.width <= 0 || rec.height <= 0 ||
        static_cast<int>(rec.x) + static_cast<int>(rec.width) > image.width ||
        static_cast<int>(rec.y) + static_cast<int>(rec.height) > image.height) {
        spdlog::warn("IMAGE: image_from_image(), rectangle provided not valid");
        return out;
    }
    const int bpp = get_pixel_data_size(1, 1, image.format);
    const int w = static_cast<int>(rec.width), h = static_cast<int>(rec.height);
    const int size = get_pixel_data_size(w, h, image.format);
    if (size <= 0) {
        return out;
    }

    out = Image{std::calloc(size, 1), w, h, 1, image.format};
    if (!out.data) {
        return {};
    }

    for (int y = 0; y < h; y++) {
        std::memcpy(static_cast<unsigned char*>(out.data) + y * w * bpp,
                    static_cast<unsigned char*>(image.data) +
                        ((y + static_cast<int>(rec.y)) * image.width + static_cast<int>(rec.x)) * bpp,
                    w * bpp);
    }
    return out;
}

void image_rotate_cw(Image* image) {
    const int size = get_pixel_data_size(image->width, image->height, image->format);
    if (!image->data || size <= 0) {
        return;
    }
    const int bpp = get_pixel_data_size(1, 1, image->format);
    const int w = image->width, h = image->height;
    auto* src = static_cast<unsigned char*>(image->data);
    auto* out = static_cast<unsigned char*>(std::malloc(size));
    if (!out) {
        return;
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            std::memcpy(out + (x * h + (h - y - 1)) * bpp, src + (y * w + x) * bpp, bpp);
        }
    }
    std::free(image->data);
    image->data = out;
    image->width = h;
    image->height = w;
}

static Color get_pixel(const unsigned char* p, int format) {
    switch (format) {
    case pixel_format::GRAYSCALE:
        return {p[0], p[0], p[0], 255};
    case pixel_format::GRAY_ALPHA:
        return {p[0], p[0], p[0], p[1]};
    case pixel_format::RGB8:
        return {p[0], p[1], p[2], 255};
    default:
        return {p[0], p[1], p[2], p[3]};
    }
}

void image_format(Image* image, int new_format) {
    if (!image->data || image->format == new_format) {
        return;
    }
    if (new_format != pixel_format::RGBA8) { // ponytail: only conversion the game needs; add others when used
        spdlog::warn("IMAGE: image_format() only converts to RGBA8");
        return;
    }
    const int n = image->width * image->height;
    const int bpp = get_pixel_data_size(1, 1, image->format);
    auto* src = static_cast<unsigned char*>(image->data);
    auto* out = static_cast<Color*>(std::malloc(n * sizeof(Color)));
    for (int i = 0; i < n; i++) {
        out[i] = get_pixel(src + i * bpp, image->format);
    }
    std::free(image->data);
    image->data = out;
    image->format = new_format;
    image->mipmaps = 1;
}

Rect get_image_alpha_border(Image image, float threshold) {
    if (!image.data) {
        return {};
    }
    const int bpp = get_pixel_data_size(1, 1, image.format);
    const auto* px = static_cast<const unsigned char*>(image.data);
    const auto limit = static_cast<unsigned char>(threshold * 255.0f);
    int x_min = 65536, x_max = 0, y_min = 65536, y_max = 0;
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            if (get_pixel(px + (y * image.width + x) * bpp, image.format).a > limit) {
                x_min = x < x_min ? x : x_min;
                x_max = x > x_max ? x : x_max;
                y_min = y < y_min ? y : y_min;
                y_max = y > y_max ? y : y_max;
            }
        }
    }
    if (x_min == 65536) {
        return {};
    }
    return {static_cast<float>(x_min), static_cast<float>(y_min), static_cast<float>(x_max + 1 - x_min),
            static_cast<float>(y_max + 1 - y_min)};
}

static Texture load_texture(const char* path, bool mipmaps) {
    Image image = load_image(path);
    Texture tex = image.data ? load_texture_from_image(image, mipmaps) : Texture{};
    unload_image(image);
    return tex;
}

Texture load_texture(const char* path) {
    return load_texture(path, false);
}

Texture load_texture_mipmapped(const char* path, int filter) {
    Texture tex = load_texture(path, true);
    set_texture_filter(tex, filter);
    return tex;
}
