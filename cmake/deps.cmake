# Sqlite3
if(NOT WIN32 AND NOT ANDROID AND NOT EMSCRIPTEN)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SQLITE3 QUIET sqlite3)
  endif()
endif()

if(ANDROID OR EMSCRIPTEN)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST   OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.4
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(SDL3)
else()
  find_package(SDL3 QUIET CONFIG REQUIRED)
  if(SDL3_FOUND)
    message(STATUS "Using system SDL3")
  else()
    message(STATUS "Fetching SDL3 from source")
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST   OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
      SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG        release-3.4.4
      GIT_SHALLOW    TRUE
      OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(SDL3)
  endif()
endif()

if(SQLITE3_FOUND)
  message(STATUS "Using system SQLite3")
  add_library(SQLite3_lib INTERFACE IMPORTED)
  target_include_directories(SQLite3_lib INTERFACE ${SQLITE3_INCLUDE_DIRS})
  target_link_libraries(SQLite3_lib INTERFACE ${SQLITE3_LINK_LIBRARIES})
else()
  message(STATUS "System SQLite3 not found, fetching amalgamation")
  FetchContent_Declare(
      sqlite3
      URL https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip
  )
  FetchContent_MakeAvailable(sqlite3)

  add_library(SQLite3_lib STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
  target_include_directories(SQLite3_lib PUBLIC ${sqlite3_SOURCE_DIR})
endif()

# raylib
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(PLATFORM "SDL" CACHE STRING "" FORCE)
set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)
set(SUPPORT_CUSTOM_FRAME_CONTROL ON CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_JPG ON CACHE BOOL "" FORCE)
set(GRAPHICS GRAPHICS_API_VULKAN_14 ON CACHE ENUM "" FORCE)
if(ANDROID OR EMSCRIPTEN)
  set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
endif()
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/rygo6/raylib.git
  GIT_TAG        rlvk-pipeline-cache
  GIT_SHALLOW    TRUE
  PATCH_COMMAND  ${CMAKE_COMMAND} -D RAYLIB_PATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/raylib-sdl-vulkan.patch -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplyRaylibPatch.cmake
)
FetchContent_GetProperties(raylib)
FetchContent_MakeAvailable(raylib)

# shaderc (runtime GLSL->SPIR-V compiler the Vulkan raylib backend loads as shaderc_shared.dll --
# see rlvk.h's rlvkLoadShaderc(). Without it every custom shader silently falls back to the
# embedded default shader, so it's a hard requirement, not an optional extra.
# Versions pinned from shaderc's own DEPS file so glslang/SPIRV-Tools/SPIRV-Headers are a
# mutually-compatible set; bump all four together.
#
# Only FETCH glslang/SPIRV-Tools/SPIRV-Headers here (FetchContent_Populate, not
# MakeAvailable): shaderc's own third_party/CMakeLists.txt add_subdirectory()s all three
# itself (guarded by if(NOT TARGET ...), in the specific order it needs SPIRV-Tools linked
# into glslang), via the SHADERC_*_DIR variables below. Calling MakeAvailable on them here
# too would add_subdirectory each one twice -> duplicate target names -> fatal CMake error.
if(NOT ANDROID AND NOT EMSCRIPTEN)
  FetchContent_Declare(
    SPIRV-Headers
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
  )
  FetchContent_Populate(SPIRV-Headers)

  FetchContent_Declare(
    SPIRV-Tools
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
  )
  FetchContent_Populate(SPIRV-Tools)

  FetchContent_Declare(
    glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
  )
  FetchContent_Populate(glslang)

  set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
  set(SHADERC_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
  set(SHADERC_SKIP_INSTALL ON CACHE BOOL "" FORCE)
  set(SHADERC_SKIP_COPYRIGHT_CHECK ON CACHE BOOL "" FORCE)
  # Not required for correctness (shaderc's own script already forces SPIRV_SKIP_TESTS from
  # SHADERC_SKIP_TESTS) -- just skips SPIRV-Tools' standalone CLI tools (spirv-opt.exe etc.)
  # that this project never uses, to cut build time.
  set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
  # shaderc's third_party/CMakeLists.txt reads these instead of the standard FetchContent
  # <name>_SOURCE_DIR convention, so point them at the sources fetched above.
  set(SHADERC_GLSLANG_DIR ${glslang_SOURCE_DIR} CACHE PATH "" FORCE)
  set(SHADERC_SPIRV_TOOLS_DIR ${spirv-tools_SOURCE_DIR} CACHE PATH "" FORCE)
  set(SHADERC_SPIRV_HEADERS_DIR ${spirv-headers_SOURCE_DIR} CACHE PATH "" FORCE)
  FetchContent_Declare(
    shaderc
    GIT_REPOSITORY https://github.com/google/shaderc.git
    GIT_TAG        49a8724d561c13db22b52f99f2a0e2707a9a9e3c
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/FixShadercGlslangInstall.cmake
  )
  FetchContent_MakeAvailable(shaderc)
endif()

# RapidJSON
set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(rapidjson)

# tomlplusplus
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tomlplusplus)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spdlog)

# Lua
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/marovira/lua.git
    GIT_TAG 5.4.8
    GIT_SHALLOW TRUE
    PATCH_COMMAND sed -i "s/cmake_minimum_required(VERSION 3\\.30)/cmake_minimum_required(VERSION 3.5)/" CMakeLists.txt || true
)
FetchContent_MakeAvailable(lua)

# Sol2
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.5.0
    GIT_SHALLOW TRUE
    PATCH_COMMAND git apply ${CMAKE_SOURCE_DIR}/cmake/patches/sol2-android-noexcept.patch || true
        COMMAND sed -i "s/cmake_minimum_required(VERSION 3\\.26\\.0)/cmake_minimum_required(VERSION 3.5)/" CMakeLists.txt || true
)
FetchContent_MakeAvailable(sol2)

if(NOT ANDROID AND NOT EMSCRIPTEN)
  set(ZLIB_USE_STATIC_LIBS ON)
  if(WIN32)
    set(CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE ON CACHE BOOL "" FORCE)
    set(CPPTRACE_UNWIND_WITH_DBGHELP        ON CACHE BOOL "" FORCE)
  endif()
  FetchContent_Declare(
      cpptrace
      GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
      GIT_TAG        main
      GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(cpptrace)
endif()

# libsndfile
if(ANDROID OR EMSCRIPTEN OR WIN32)
  message(STATUS "Fetching libogg + libvorbis for ${CMAKE_SYSTEM_NAME} (needed by libsndfile)")
  FetchContent_Declare(
      ogg
      GIT_REPOSITORY https://github.com/xiph/ogg.git
      GIT_TAG v1.3.5
      GIT_SHALLOW TRUE
      PATCH_COMMAND sed -i "s/cmake_minimum_required(VERSION 2\\.8\\.12)/cmake_minimum_required(VERSION 3.5)/" CMakeLists.txt || true
  )
  FetchContent_MakeAvailable(ogg)
  set(OGG_INCLUDE_DIR "${ogg_SOURCE_DIR}/include" CACHE PATH "" FORCE)
  set(OGG_LIBRARY "ogg" CACHE STRING "" FORCE)

  FetchContent_Declare(
      vorbis
      GIT_REPOSITORY https://github.com/xiph/vorbis.git
      GIT_TAG v1.3.7
      GIT_SHALLOW TRUE
      PATCH_COMMAND sed -i "s/cmake_minimum_required(VERSION 2\\.8\\.12)/cmake_minimum_required(VERSION 3.5)/" CMakeLists.txt || true
  )
  FetchContent_MakeAvailable(vorbis)
  set(Vorbis_Vorbis_INCLUDE_DIR "${vorbis_SOURCE_DIR}/include" CACHE PATH "" FORCE)
  set(Vorbis_Vorbis_LIBRARY "vorbis" CACHE STRING "" FORCE)
  set(Vorbis_File_INCLUDE_DIR "${vorbis_SOURCE_DIR}/include" CACHE PATH "" FORCE)
  set(Vorbis_File_LIBRARY "vorbisfile" CACHE STRING "" FORCE)
  set(Vorbis_Enc_INCLUDE_DIR "${vorbis_SOURCE_DIR}/include" CACHE PATH "" FORCE)
  set(Vorbis_Enc_LIBRARY "vorbisenc" CACHE STRING "" FORCE)
  if(NOT TARGET Vorbis::vorbis)
    add_library(Vorbis::vorbis ALIAS vorbis)
  endif()
  if(NOT TARGET Vorbis::vorbisenc)
    add_library(Vorbis::vorbisenc ALIAS vorbisenc)
  endif()
  if(NOT TARGET Vorbis::vorbisfile)
    add_library(Vorbis::vorbisfile ALIAS vorbisfile)
  endif()

  message(STATUS "Fetching libflac for ${CMAKE_SYSTEM_NAME} (needed by libsndfile)")
  set(BUILD_CXXLIBS OFF CACHE BOOL "" FORCE)
  set(BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(INSTALL_MANPAGES OFF CACHE BOOL "" FORCE)
  set(WITH_OGG ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
      flac
      GIT_REPOSITORY https://github.com/xiph/flac.git
      GIT_TAG 1.4.3
      GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(flac)
  set(FLAC_INCLUDE_DIR "${flac_SOURCE_DIR}/include" CACHE PATH "" FORCE)
  set(FLAC_LIBRARY "FLAC" CACHE STRING "" FORCE)

  message(STATUS "Fetching libopus for ${CMAKE_SYSTEM_NAME} (needed by libsndfile)")
  FetchContent_Declare(
      opus
      GIT_REPOSITORY https://github.com/xiph/opus.git
      GIT_TAG v1.5.2
      GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(opus)
  set(OPUS_INCLUDE_SHIM_DIR "${CMAKE_BINARY_DIR}/opus_include_shim")
  file(MAKE_DIRECTORY "${OPUS_INCLUDE_SHIM_DIR}")
  if(NOT EXISTS "${OPUS_INCLUDE_SHIM_DIR}/opus")
    file(CREATE_LINK "${opus_SOURCE_DIR}/include" "${OPUS_INCLUDE_SHIM_DIR}/opus" SYMBOLIC)
  endif()
  target_include_directories(opus INTERFACE "$<BUILD_INTERFACE:${OPUS_INCLUDE_SHIM_DIR}>")
  set(OPUS_INCLUDE_DIR "${OPUS_INCLUDE_SHIM_DIR}" CACHE PATH "" FORCE)
  set(OPUS_LIBRARY "opus" CACHE STRING "" FORCE)

  message(STATUS "Fetching libsndfile from source (${CMAKE_SYSTEM_NAME}, OGG/Vorbis/FLAC/Opus/MP3)")
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(ENABLE_EXTERNAL_LIBS ON CACHE BOOL "" FORCE)
  set(ENABLE_MPEG ON CACHE BOOL "" FORCE)
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
      libsndfile
      GIT_REPOSITORY https://github.com/libsndfile/libsndfile.git
      GIT_TAG master
      GIT_SHALLOW TRUE
    )
  FetchContent_MakeAvailable(libsndfile)
else()
  message(STATUS "Fetching libsndfile from source")
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(ENABLE_EXTERNAL_LIBS ON CACHE BOOL "" FORCE)
  set(ENABLE_MPEG ON CACHE BOOL "" FORCE)
  set(ENABLE_OPUS ON CACHE BOOL "" FORCE)
  set(ENABLE_FLAC ON CACHE BOOL "" FORCE)
  set(ENABLE_VORBIS ON CACHE BOOL "" FORCE)
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
      libsndfile
      GIT_REPOSITORY https://github.com/libsndfile/libsndfile.git
      GIT_TAG master
      GIT_SHALLOW TRUE
    )
  FetchContent_MakeAvailable(libsndfile)
endif()

# libsamplerate
if(ANDROID OR EMSCRIPTEN)
  message(STATUS "Fetching libsamplerate from source (${CMAKE_SYSTEM_NAME})")
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(LIBSAMPLERATE_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(LIBSAMPLERATE_INSTALL OFF CACHE BOOL "" FORCE)
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
      libsamplerate
      GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
      GIT_TAG master
      GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(libsamplerate)
else()
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SAMPLERATE QUIET samplerate)
  endif()

  if(SAMPLERATE_FOUND)
    message(STATUS "Using system libsamplerate")
    add_library(samplerate INTERFACE IMPORTED)
    target_include_directories(samplerate INTERFACE ${SAMPLERATE_INCLUDE_DIRS})
    target_link_libraries(samplerate INTERFACE ${SAMPLERATE_LINK_LIBRARIES})
    target_link_directories(samplerate INTERFACE ${SAMPLERATE_LIBRARY_DIRS})
  else()
    message(STATUS "System libsamplerate not found, fetching from source")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(LIBSAMPLERATE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LIBSAMPLERATE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
              libsamplerate
              GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
              GIT_TAG master
              GIT_SHALLOW TRUE
          )
    FetchContent_MakeAvailable(libsamplerate)
  endif()
endif()

# FFmpeg
if(WIN32)
  FetchContent_Declare(
        ffmpeg_prebuilt
        URL https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip
    )
  FetchContent_MakeAvailable(ffmpeg_prebuilt)

  file(GLOB_RECURSE FFMPEG_LIBS "${ffmpeg_prebuilt_SOURCE_DIR}/*.lib")
  message(STATUS "FFmpeg prebuilt source dir: ${ffmpeg_prebuilt_SOURCE_DIR}")
  message(STATUS "FFmpeg libs found: ${FFMPEG_LIBS}")

  set(FFMPEG_INCLUDE_DIR "${ffmpeg_prebuilt_SOURCE_DIR}/include")
  set(FFMPEG_LIB_DIR     "${ffmpeg_prebuilt_SOURCE_DIR}/lib")

  foreach(_lib avformat avcodec avutil swscale swresample)
    set(_libfile "${FFMPEG_LIB_DIR}/lib${_lib}.dll.a")
    if(NOT EXISTS "${_libfile}")
      message(FATAL_ERROR "FFmpeg lib not found: ${_libfile}")
    endif()
    add_library(FFmpeg::${_lib} SHARED IMPORTED)
    set_target_properties(FFmpeg::${_lib} PROPERTIES
            IMPORTED_LOCATION             "${FFMPEG_LIB_DIR}/../bin/${_lib}.dll"
            IMPORTED_IMPLIB               "${_libfile}"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
        )
  endforeach()
elseif(EMSCRIPTEN)
  message(STATUS "FFmpeg disabled on Emscripten -- video/av features unavailable until ported")
  foreach(_lib avformat avcodec avutil swscale swresample)
    add_library(FFmpeg::${_lib} INTERFACE IMPORTED)
    set_target_properties(FFmpeg::${_lib} PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/src/libs")
  endforeach()
elseif(ANDROID)
  # FFmpeg must be cross-compiled for Android separately.
  # Set ANDROID_FFMPEG_PREFIX to a directory containing:
  #   <prefix>/include/  (libavformat/, libavcodec/, ...)
  #   <prefix>/lib/      (libavformat.a, libavcodec.a, ...)
  # Build with: https://github.com/arthenica/ffmpeg-kit or compile manually with NDK.
  if(NOT DEFINED ANDROID_FFMPEG_PREFIX)
    if(DEFINED ENV{ANDROID_FFMPEG_PREFIX})
      set(ANDROID_FFMPEG_PREFIX "$ENV{ANDROID_FFMPEG_PREFIX}")
    else()
      message(FATAL_ERROR
        "ANDROID_FFMPEG_PREFIX must point to a prebuilt FFmpeg directory for Android arm64-v8a.\n"
        "Expected layout:\n"
        "  <prefix>/include/  (libavformat, libavcodec, libavutil, libswscale, libswresample headers)\n"
        "  <prefix>/lib/      (libavformat.a, libavcodec.a, libavutil.a, libswscale.a, libswresample.a)\n"
        "Pass it as: cmake -DANDROID_FFMPEG_PREFIX=/path/to/ffmpeg ...")
    endif()
  endif()

  set(_ffmpeg_inc "${ANDROID_FFMPEG_PREFIX}/include")
  set(_ffmpeg_lib "${ANDROID_FFMPEG_PREFIX}/lib")
  message(STATUS "Using Android FFmpeg from ${ANDROID_FFMPEG_PREFIX}")

  foreach(_lib avformat avcodec avutil swscale swresample)
    set(_libfile "${_ffmpeg_lib}/lib${_lib}.a")
    if(NOT EXISTS "${_libfile}")
      message(FATAL_ERROR "FFmpeg lib not found: ${_libfile}")
    endif()
    add_library(FFmpeg::${_lib} STATIC IMPORTED)
    set_target_properties(FFmpeg::${_lib} PROPERTIES
      IMPORTED_LOCATION             "${_libfile}"
      INTERFACE_INCLUDE_DIRECTORIES "${_ffmpeg_inc}"
    )
  endforeach()
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(AVFORMAT REQUIRED libavformat)
  pkg_check_modules(AVCODEC  REQUIRED libavcodec)
  pkg_check_modules(AVUTIL   REQUIRED libavutil)
  pkg_check_modules(SWSCALE  REQUIRED libswscale)
  pkg_check_modules(SWRESAMPLE REQUIRED libswresample)

  foreach(_lib avformat avcodec avutil swscale swresample)
    string(TOUPPER ${_lib} _LIB)
    add_library(FFmpeg::${_lib} INTERFACE IMPORTED)
    target_include_directories(FFmpeg::${_lib} INTERFACE ${${_LIB}_INCLUDE_DIRS})
    target_link_libraries(FFmpeg::${_lib}      INTERFACE ${${_LIB}_LINK_LIBRARIES})
    target_link_directories(FFmpeg::${_lib}    INTERFACE ${${_LIB}_LIBRARY_DIRS})
  endforeach()

  message(STATUS "Using system FFmpeg")
  message(STATUS "  libavformat ${AVFORMAT_VERSION}")
  message(STATUS "  libavcodec  ${AVCODEC_VERSION}")
  message(STATUS "  libavutil   ${AVUTIL_VERSION}")
  message(STATUS "  libswscale  ${SWSCALE_VERSION}")
  message(STATUS "  libswresample ${SWRESAMPLE_VERSION}")
endif()

# RtAudio
if(EMSCRIPTEN)
  message(STATUS "RtAudio disabled on Emscripten -- audio backend unavailable until ported")
  add_library(rtaudio INTERFACE IMPORTED)
elseif(ANDROID)
  # Android uses AAudio directly -- RtAudio not needed
  add_library(rtaudio INTERFACE IMPORTED)
else()
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(RTAUDIO_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(RTAUDIO_TARGETNAME_UNINSTALL "rtaudio_uninstall" CACHE STRING "" FORCE)
  if(WIN32)
    set(RTAUDIO_API_WASAPI ON CACHE BOOL "" FORCE)
    set(RTAUDIO_API_DS ON CACHE BOOL "" FORCE)
    set(RTAUDIO_API_ASIO ON CACHE BOOL "" FORCE)
  elseif(UNIX AND NOT APPLE)
    # ALSA/PulseAudio/JACK are auto-detected by RtAudio's own CMakeLists;
    # OSS defaults off outside BSD, so force it on explicitly for Linux --
    # but only where the OSS4 ioctls RtAudio needs are actually present,
    # since many modern distros ship a trimmed ALSA-compat soundcard.h.
    include(CheckSymbolExists)
    check_symbol_exists(SNDCTL_DSP_HALT "sys/soundcard.h" HAVE_OSS4_HEADERS)
    if(HAVE_OSS4_HEADERS)
      set(RTAUDIO_API_OSS ON CACHE BOOL "" FORCE)
    else()
      set(RTAUDIO_API_OSS OFF CACHE BOOL "" FORCE)
      message(STATUS "OSS4 headers not found (sys/soundcard.h missing SNDCTL_DSP_HALT) -- RtAudio OSS backend disabled")
    endif()
  endif()
  FetchContent_Declare(
    rtaudio
    GIT_REPOSITORY https://github.com/thestk/rtaudio.git
    GIT_TAG 6.0.1
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(rtaudio)
endif()

# PortAudio (Windows only -- WDM-KS / MME backends; the other Windows APIs
# -- DirectSound/ASIO/WASAPI -- are handled by RtAudio above)
if(WIN32)
  message(STATUS "Fetching PortAudio from source (WMME + WDM-KS only)")
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(PA_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(PA_BUILD_STATIC ON CACHE BOOL "" FORCE)
  set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(PA_USE_WMME ON CACHE BOOL "" FORCE)
  set(PA_USE_WDMKS ON CACHE BOOL "" FORCE)
  set(PA_USE_DS OFF CACHE BOOL "" FORCE)
  set(PA_USE_WASAPI OFF CACHE BOOL "" FORCE)
  set(PA_USE_ASIO OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    portaudio
    GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
    GIT_TAG b0fe9de7ec86ebe5a26086f1d662ab74d7ebfae4
  )
  FetchContent_MakeAvailable(portaudio)
else()
  add_library(portaudio INTERFACE IMPORTED)
endif()
