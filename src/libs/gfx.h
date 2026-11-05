#pragma once

// no particular graphics backend here

#include <cstdint>

#include "gfx/color_utils.h"
#include "gfx/gfx_types.h"
#include "math/math_types.h"
#if defined(PLATFORM_ANDROID) || defined(YATAIDON_PLATFORM_IOS)
#include "gfx/sdl_compat.h"
#endif
#include <algorithm>
#include <numbers>
#include <span>
#include <string>
#include <vector>

inline constexpr float deg2rad = std::numbers::pi_v<float> / 180.0f;

inline Color fade(Color c, float alpha) {
    return Color(c.r, c.g, c.b, static_cast<uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f));
}

namespace window_flag {
    inline constexpr unsigned RESIZABLE = 0x00000004;
    inline constexpr unsigned VSYNC = 0x00000040;
    inline constexpr unsigned ALWAYS_RUN = 0x00000100;
} // namespace window_flag

namespace texture_filter {
    inline constexpr int BILINEAR = 1;
    inline constexpr int TRILINEAR = 2;
} // namespace texture_filter

namespace texture_wrap {
    inline constexpr int CLAMP = 1;
} // namespace texture_wrap

namespace blend_mode {
    inline constexpr int ADDITIVE = 1;
    inline constexpr int CUSTOM_SEPARATE = 7;
} // namespace blend_mode

namespace shader_uniform {
    inline constexpr int FLOAT = 0;
    inline constexpr int VEC2 = 1;
    inline constexpr int VEC3 = 2;
} // namespace shader_uniform

namespace pixel_format {
    inline constexpr int GRAYSCALE = 1;
    inline constexpr int GRAY_ALPHA = 2;
    inline constexpr int RGB8 = 4;
    inline constexpr int RGBA8 = 7;
} // namespace pixel_format

namespace cull_face {
    inline constexpr int FRONT = 0;
    inline constexpr int BACK = 1;
} // namespace cull_face

namespace material_map {
    inline constexpr int DIFFUSE = 0;
}

// printf-style; returns a rotating static buffer like raylib's TextFormat.
__attribute__((format(printf, 1, 2))) const char* text_format(const char* fmt, ...);

void draw_rectangle(int x, int y, int w, int h, Color color);
void draw_rectangle(Rect rec, Color color);
void draw_rectangle_rounded(Rect rec, float roundness, int segments, Color color);
void draw_rectangle_rounded_lines_ex(Rect rec, float roundness, int segments, float thickness, Color color);
void draw_rectangle_lines(int x, int y, int w, int h, Color color);
void draw_rectangle_lines_ex(Rect rec, float thickness, Color color);
void draw_line(int x0, int y0, int x1, int y1, Color color);
void draw_model(Model model, Vec3 pos, float scale, Color tint);
void draw_text_ex(Font font, const char* text, Vec2 pos, float font_size, float spacing, Color tint);
void draw_texture_pro(Texture tex, Rect src, Rect dst, Vec2 origin, float rotation, Color tint);
void draw_text(const char* text, int x, int y, int font_size, Color color);
int measure_text(const char* text, int font_size);
void set_window_flags(unsigned flags);
void quiet_raylib_log();
void init_window(int width, int height, const char* title);
void close_window();
bool window_should_close();
void poll_window_events();
void disable_exit_key();
void toggle_fullscreen();
void toggle_borderless_windowed();
bool is_window_fullscreen();
void set_window_size(int width, int height);
int get_screen_width();
int get_screen_height();
int get_render_width();
int get_render_height();
int get_monitor_refresh_rate();
void begin_drawing();
void end_drawing();
void swap_screen_buffer();
Texture load_texture(const char* path);
Texture load_texture_from_image(Image image, bool mipmaps = false);
void unload_texture(Texture tex);
void set_texture_filter(Texture tex, int filter);
void set_texture_wrap(Texture tex, int wrap);
Texture load_texture_mipmapped(const char* path, int filter = texture_filter::TRILINEAR);
Vec2 measure_text_ex(const Font& font, const char* text, float font_size, float spacing);
// Loads printable ASCII; falls back to the default font.
Font load_font(const char* path, int font_size);
Font load_font_from_memory(std::span<const unsigned char> data, int font_size);
// Built-in CJK + Latin TTF (assets/fonts/default.ttf), embedded at build time.
std::span<const unsigned char> default_font_ttf();
void unload_font(Font font);
// Rasterizes the codepoints the font has; each glyph's GRAYSCALE image is the
// caller's to free with unload_image.
std::vector<GlyphInfo> load_font_data(std::span<const unsigned char> data, int font_size,
                                      std::span<const int> codepoints);
// Packs glyph images into an atlas; *recs receives a malloc'd array of glyph_count rects.
Image gen_image_font_atlas(const GlyphInfo* glyphs, Rect** recs, int glyph_count, int font_size, int padding);
Font get_font_default();
int get_glyph_index(const Font& font, int codepoint);
int get_codepoint_next(const char* text, int* codepoint_size);
const char* codepoint_to_utf8(int codepoint, int* utf8_size);
void begin_scissor_mode(int x, int y, int width, int height);
void end_scissor_mode();
void begin_blend_mode(int mode);
// Factors for blend_mode::CUSTOM_SEPARATE: color src_alpha/one_minus_src_alpha,
// alpha one/one_minus_src_alpha (keeps render-target alpha correct).
void set_separate_alpha_blend();
void set_backface_culling(bool enabled);
void set_cull_face(int face);
void end_blend_mode();
void begin_mode_2d(Camera2D camera);
void end_mode_2d();
void begin_mode_3d(Camera3D camera);
void end_mode_3d();
Vec2 get_screen_to_world_2d(Vec2 position, Camera2D camera);
Vec2 get_world_to_screen_2d(Vec2 position, Camera2D camera);
void unload_shader(Shader shader);
bool is_shader_valid(Shader shader);
int get_shader_location(Shader shader, const char* name);
void set_shader_value(Shader shader, int loc, const void* value, int uniform_type);
void begin_shader_mode(Shader shader);
void end_shader_mode();
Image load_image(const char* path);
// PNG/JPEG/BMP file contents
Image load_image_from_memory(std::span<const unsigned char> data);
Image load_image_from_texture(Texture tex);
Image load_image_from_screen();
void unload_image(Image image);
bool export_image(Image image, const char* path);
Image image_copy(Image image);
Image image_from_image(Image image, Rect rec);
void image_format(Image* image, int new_format);
void image_rotate_cw(Image* image);
Rect get_image_alpha_border(Image image, float threshold);
int get_pixel_data_size(int width, int height, int format);
void update_texture(Texture tex, const void* pixels);
bool is_texture_valid(Texture tex);
// Whole file, or empty on failure.
std::vector<unsigned char> load_file_bytes(const char* path);
RenderTarget load_render_texture(int width, int height);
void unload_render_texture(RenderTarget target);
void begin_texture_mode(RenderTarget target);
void end_texture_mode();
Model load_model(const char* path);
void unload_model(Model model);
ModelAnimation* load_model_animations(const char* path, int* count);
void unload_model_animations(ModelAnimation* anims, int count);
void update_model_animation(Model model, ModelAnimation anim, float frame);
bool get_mesh_bounds(const Mesh& mesh, Vec3& lower, Vec3& upper);
void draw_mesh(Mesh mesh, Material material, Mat4 transform);
void update_mesh_buffer(Mesh mesh, int index, const void* data, int data_size, int offset);
void set_material_texture(Material* material, int map_type, Texture texture);
void clear_background(Color color);
Image gen_image_color(int w, int h, Color color);
Shader load_shader(const char* vs_path, const char* fs_path);
