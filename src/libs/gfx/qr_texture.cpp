#include "qr_texture.h"

#include <qrcodegen.hpp>
#include <vector>

static constexpr int QR_QUIET_ZONE = 2;

std::string access_code_login_url(const std::string& access_code) {
    return "https://yataidon.tjadataba.se/login?code=" + access_code;
}

Texture make_qr_texture(const std::string& text) {
    const auto qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int side = qr.getSize() + (QR_QUIET_ZONE * 2);
    std::vector<unsigned char> pixels(static_cast<size_t>(side) * side * 4, 255);
    for (int y = 0; y < qr.getSize(); y++) {
        for (int x = 0; x < qr.getSize(); x++) {
            if (qr.getModule(x, y)) {
                unsigned char* px = &pixels[((static_cast<size_t>(y + QR_QUIET_ZONE) * side) + x + QR_QUIET_ZONE) * 4];
                px[0] = px[1] = px[2] = 0;
            }
        }
    }
    Image image{};
    image.data = pixels.data(); // texture upload copies
    image.width = image.height = side;
    image.mipmaps = 1;
    image.format = pixel_format::RGBA8;
    return load_texture_from_image(image);
}
