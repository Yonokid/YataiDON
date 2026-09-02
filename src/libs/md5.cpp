#include "md5.h"

#include <cstring>

namespace {

const uint32_t MD5_SHIFT[64] = {
    7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
    5, 9,14,20, 5, 9,14,20, 5, 9,14,20, 5, 9,14,20,
    4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
    6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21 };

const uint32_t MD5_K[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391 };

} // namespace

void md5(const uint8_t* data, size_t len, uint32_t out[4]) {
    out[0] = 0x67452301; out[1] = 0xefcdab89;
    out[2] = 0x98badcfe; out[3] = 0x10325476;

    size_t padded = ((len + 8) / 64 + 1) * 64 - 8;
    std::vector<uint8_t> msg(padded + 8, 0);
    memcpy(msg.data(), data, len);
    msg[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) msg[padded + i] = (uint8_t)(bits >> (i * 8));

    for (size_t off = 0; off < msg.size(); off += 64) {
        uint32_t w[16];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)msg[off + i * 4] | ((uint32_t)msg[off + i * 4 + 1] << 8) |
                   ((uint32_t)msg[off + i * 4 + 2] << 16) | ((uint32_t)msg[off + i * 4 + 3] << 24);

        uint32_t a = out[0], b = out[1], c = out[2], d = out[3];
        for (int i = 0; i < 64; i++) {
            uint32_t f, g;
            if (i < 16)      { f = (b & c) | (~b & d);  g = i; }
            else if (i < 32) { f = (d & b) | (~d & c);  g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d;           g = (3 * i + 5) % 16; }
            else             { f = c ^ (b | ~d);        g = (7 * i) % 16; }
            uint32_t tmp = d;
            d = c;
            c = b;
            uint32_t x = a + f + MD5_K[i] + w[g];
            b = b + ((x << MD5_SHIFT[i]) | (x >> (32 - MD5_SHIFT[i])));
            a = tmp;
        }
        out[0] += a; out[1] += b; out[2] += c; out[3] += d;
    }
}

std::string md5_hexdigest(const std::vector<unsigned char>& data) {
    static const char hex_chars[] = "0123456789abcdef";

    const unsigned char zero = 0;
    const unsigned char* ptr = data.empty() ? &zero : data.data();

    uint32_t hash[4];
    md5(ptr, data.size(), hash);

    std::string result(32, '0');
    for (int word = 0; word < 4; word++) {
        for (int byte = 0; byte < 4; byte++) {
            unsigned char b = (hash[word] >> (8 * byte)) & 0xFF;
            result[(word * 4 + byte) * 2]     = hex_chars[b >> 4];
            result[(word * 4 + byte) * 2 + 1] = hex_chars[b & 0xF];
        }
    }
    return result;
}
