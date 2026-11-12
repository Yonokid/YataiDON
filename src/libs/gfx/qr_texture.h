#pragma once

#include <string>

#include "../gfx.h"

// Web login link for an access code; the QR on the site encodes the same URL.
std::string access_code_login_url(const std::string& access_code);

// Black-on-white QR with a quiet zone. The caller owns the texture (unload_texture).
Texture make_qr_texture(const std::string& text);
