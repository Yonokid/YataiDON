#pragma once
#include <string>

namespace ray {
    extern "C" {
        #include <raylib.h>
    }
}

#ifdef PLATFORM_ANDROID
#include <SDL3/SDL.h>
#endif

inline ray::Shader load_shader(const char* vs_path, const char* fs_path) {
#ifdef PLATFORM_ANDROID
    auto repath = [](const char* p) -> std::string {
        if (!p) return "";
        std::string s(p);
        const std::string prefix = "shader/";
        if (s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix)
            s.insert(prefix.size(), "es/");
        return s;
    };

    auto read_asset = [](const std::string& path) -> std::string {
        if (path.empty()) return "";
        SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "r");
        if (!io) return "";
        Sint64 size = SDL_GetIOSize(io);
        if (size <= 0) { SDL_CloseIO(io); return ""; }
        std::string buf(static_cast<size_t>(size), '\0');
        SDL_ReadIO(io, buf.data(), static_cast<size_t>(size));
        SDL_CloseIO(io);
        return buf;
    };

    std::string vs_src = vs_path ? read_asset(repath(vs_path)) : std::string{};
    std::string fs_src = fs_path ? read_asset(repath(fs_path)) : std::string{};
    return ray::LoadShaderFromMemory(
        vs_src.empty() ? nullptr : vs_src.c_str(),
        fs_src.empty() ? nullptr : fs_src.c_str()
    );
#else
    return ray::LoadShader(vs_path, fs_path);
#endif
}
