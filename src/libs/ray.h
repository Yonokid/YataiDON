#pragma once
#include <string>

namespace ray {
    extern "C" {
        #include <raylib.h>
    }
}

#if defined(PLATFORM_ANDROID) || defined(PLATFORM_VITA)
#include <SDL3/SDL.h>
#endif

inline ray::Shader load_shader(const char* vs_path, const char* fs_path) {
#ifdef PLATFORM_VITA
    // The working directory is ux0:data/YataiDON (see filesystem.cpp), but
    // shader/es/ is only bundled read-only inside the vpk itself, so it has
    // to be read from the app0: mount explicitly rather than a cwd-relative
    // path -- ux0: and app0: are different filesystem roots on Vita.
    auto repath = [](const char* p) -> std::string {
        if (!p) return "";
        std::string s(p);
        const std::string prefix = "shader/";
        if (s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix)
            s.insert(prefix.size(), "es/");
        return "app0:" + s;
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

    std::string vs_repath = vs_path ? repath(vs_path) : std::string{};
    std::string fs_repath = fs_path ? repath(fs_path) : std::string{};
    std::string vs_src = vs_path ? read_asset(vs_repath) : std::string{};
    std::string fs_src = fs_path ? read_asset(fs_repath) : std::string{};
    return ray::LoadShaderFromMemory(
        vs_src.empty() ? nullptr : vs_src.c_str(),
        fs_src.empty() ? nullptr : fs_src.c_str()
    );
#endif
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

    std::string vs_repath = vs_path ? repath(vs_path) : std::string{};
    std::string fs_repath = fs_path ? repath(fs_path) : std::string{};
    std::string vs_src = vs_path ? read_asset(vs_repath) : std::string{};
    std::string fs_src = fs_path ? read_asset(fs_repath) : std::string{};
    return ray::LoadShaderFromMemory(
        vs_src.empty() ? nullptr : vs_src.c_str(),
        fs_src.empty() ? nullptr : fs_src.c_str()
    );
#endif
#ifdef __EMSCRIPTEN__
    auto repath = [](const char* p) -> std::string {
        if (!p) return "";
        std::string s(p);
        const std::string prefix = "shader/";
        if (s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix)
            s.insert(prefix.size(), "es/");
        return s;
    };
    std::string vs_repath = vs_path ? repath(vs_path) : std::string{};
    std::string fs_repath = fs_path ? repath(fs_path) : std::string{};
    return ray::LoadShader(vs_repath.c_str(), fs_repath.c_str());
#endif
    return ray::LoadShader(vs_path, fs_path);
}
