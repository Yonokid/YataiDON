if(VITA)
  # vitasdk's newlib gates POSIX/BSD declarations (fileno, fwrite_unlocked, ...)
  # behind explicit _POSIX_C_SOURCE/_DEFAULT_SOURCE feature-test macros --
  # unlike glibc/bionic it does not expose them by default. Set directory-wide
  # so every FetchContent'd target (SDL3, spdlog, raylib, ...) picks it up too.
  add_compile_definitions(_POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
endif()

# Sqlite3
if(NOT WIN32 AND NOT ANDROID AND NOT EMSCRIPTEN AND NOT VITA)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SQLITE3 QUIET sqlite3)
  endif()
endif()

if(VITA)
  # VIDEO_VITA_PVR + PVR_PSP2 instead of VIDEO_VITA_PIB + Piglet: Piglet/PIB
  # is archived (superseded by PVR_PSP2 per its own README) and needs Sony's
  # libshacccg.suprx extracted from a registered Vita, while PVR_PSP2 is
  # actively maintained and self-contained. PVR_PSP2 doesn't ship a vitasdk
  # package, so vendor its headers (from the source repo) and prebuilt stub
  # libs (from its GitHub release) the same way everything else here is
  # FetchContent'd, instead of hand-installing into the vitasdk tree.
  FetchContent_Declare(
    pvr_psp2_headers
    GIT_REPOSITORY https://github.com/GrapheneCt/PVR_PSP2.git
    GIT_TAG        v3.9
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(pvr_psp2_headers)

  FetchContent_Declare(
    pvr_psp2_stubs
    URL https://github.com/GrapheneCt/PVR_PSP2/releases/download/v3.9/vitasdk_stubs.zip
  )
  FetchContent_MakeAvailable(pvr_psp2_stubs)

  # The actual runtime .suprx modules (as opposed to the stub .a libs linked
  # above). These are NOT a taiHEN kernel plugin like Piglet was -- Vita3K's
  # log showed it looking for them at app0:module/<name>.suprx, i.e. bundled
  # inside our own vpk and loaded as regular user modules. Bundled into the
  # vpk in CMakeLists.txt's vita_create_vpk call.
  FetchContent_Declare(
    pvr_psp2_runtime
    URL https://github.com/GrapheneCt/PVR_PSP2/releases/download/v3.9/PSVita_Release.zip
  )
  FetchContent_MakeAvailable(pvr_psp2_runtime)

  # SDL's own CMakeLists.txt gates the PVR driver on
  # check_include_file(gpu_es4/psp2_pvr_hint.h HAVE_PVR_H), which is a
  # separate configure-time probe from our app's own target_include_directories
  # and only looks at CMAKE_REQUIRED_INCLUDES -- has to be set before SDL3 is
  # fetched/configured, not after.
  list(APPEND CMAKE_REQUIRED_INCLUDES "${pvr_psp2_headers_SOURCE_DIR}/include")
  include_directories("${pvr_psp2_headers_SOURCE_DIR}/include")

  foreach(_pvr_stub_dir
      libgpu_es4_ext_stub_vitasdk.a
      libIMGEGL_stub_vitasdk.a
      libGLESv2_stub_vitasdk.a
      libGLESv1_CM_stub_vitasdk.a)
    list(APPEND CMAKE_LIBRARY_PATH "${pvr_psp2_stubs_SOURCE_DIR}/${_pvr_stub_dir}")
  endforeach()
  link_directories(
    "${pvr_psp2_stubs_SOURCE_DIR}/libgpu_es4_ext_stub_vitasdk.a"
    "${pvr_psp2_stubs_SOURCE_DIR}/libIMGEGL_stub_vitasdk.a"
    "${pvr_psp2_stubs_SOURCE_DIR}/libGLESv2_stub_vitasdk.a"
    "${pvr_psp2_stubs_SOURCE_DIR}/libGLESv1_CM_stub_vitasdk.a"
  )
endif()

if(ANDROID OR EMSCRIPTEN OR VITA)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST   OFF CACHE BOOL "" FORCE)
  if(VITA)
    set(VIDEO_VITA_PVR ON CACHE BOOL "" FORCE)
    set(VIDEO_VITA_PIB OFF CACHE BOOL "" FORCE)
  endif()
  set(_sdl3_patch_command "")
  if(VITA)
    # SDL's PVR loader (SDL_vitagles_pvr.c) explicitly loads libfios2.suprx
    # and libc.suprx from vs0:sys/external/ before our bundled
    # libgpu_es4_ext.suprx, but never loads os0:us/libgpu_es4.suprx -- the
    # actual kernel-side PowerVR driver libgpu_es4_ext links against
    # (SceGpuEs4User: PVRSRVConnect/PVRSRVDisconnect/PVRSRVAllocUserModeMem).
    # Confirmed present in an imported Vita3K firmware dump at that exact
    # path but never loaded, which is why libgpu_es4_ext's module_start fails
    # on those 3 NIDs. Untested hypothesis, not a documented/confirmed SDL
    # bug -- inserting the missing load call ourselves to try it. Using a
    # real patch file (like the sol2-android-noexcept patch below) instead
    # of an inline sed script -- CMake's PATCH_COMMAND list-argument parsing
    # has repeatedly mangled complex sed scripts (it splits on unescaped
    # ';', '<'/'>', and even embedded '"' as if they were its own list/shell
    # syntax), so a patch file sidesteps that entirely.
    set(_sdl3_patch_command
      git apply ${CMAKE_SOURCE_DIR}/cmake/patches/sdl3-vita-load-gpu-es4.patch || true
    )
  endif()

  FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.4
    GIT_SHALLOW    TRUE
    PATCH_COMMAND  ${_sdl3_patch_command}
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
      # sys/ioctl.h is pulled in unconditionally by os_unix.c's "standard
      # includes" block, but nothing in the amalgamation actually calls
      # ioctl() (only reachable behind SQLITE_ENABLE_LOCKING_STYLE, which
      # defaults off outside Apple) -- vitasdk's newlib has no ioctl.h at all.
      # HAVE_READLINK is #define'd unconditionally (no #ifndef guard) right
      # after this for every non-VxWorks target, assuming any POSIX-ish
      # system has a real readlink() -- vitasdk's newlib declares the
      # prototype (now that _POSIX_C_SOURCE is set) but never implements it.
      # Delete the line rather than redefine to 0: the aSyscall table below
      # gates on "#if defined(HAVE_READLINK)", which is still true for a
      # macro defined as 0.
      # (Two -e expressions, not one semicolon-joined script: CMake treats
      # an unescaped ';' as its own list separator and silently drops
      # whatever comes after it, so the combined script silently ran as
      # just the first expression.)
      PATCH_COMMAND sed -i -e "/ioctl\\.h/d" -e "/define HAVE_READLINK 1/d" sqlite3.c || true
  )
  FetchContent_MakeAvailable(sqlite3)

  add_library(SQLite3_lib STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
  target_include_directories(SQLite3_lib PUBLIC ${sqlite3_SOURCE_DIR})
  if(VITA)
    # vitasdk's newlib also has no sys/mman.h; mmap()/munmap() are genuinely
    # used by the WAL + mmap-I/O path, so (unlike ioctl.h) that one can't just
    # be patched out -- disable it via SQLite's own compile-time knobs instead.
    # SQLITE_WASI is SQLite's own official knob for "reduced-POSIX target
    # missing fchown/mmap/mremap" -- exactly our situation, despite Vita not
    # actually being WASM/WASI. SQLITE_OMIT_LOAD_EXTENSION drops dlopen/dlsym
    # usage entirely (no dynamic loading on Vita homebrew anyway).
    target_compile_definitions(SQLite3_lib PRIVATE
      SQLITE_OMIT_WAL SQLITE_MAX_MMAP_SIZE=0 SQLITE_WASI SQLITE_OMIT_LOAD_EXTENSION)
  endif()
endif()

# raylib
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(PLATFORM "SDL" CACHE STRING "" FORCE)
set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)
set(SUPPORT_CUSTOM_FRAME_CONTROL ON CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_JPG ON CACHE BOOL "" FORCE)
if(VITA)
  set(OPENGL_VERSION "ES 2.0" CACHE STRING "" FORCE)
elseif(ANDROID OR EMSCRIPTEN)
  set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
endif()
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        master
  GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(raylib)
FetchContent_MakeAvailable(raylib)

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

if(NOT ANDROID AND NOT EMSCRIPTEN AND NOT VITA)
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
if(ANDROID OR EMSCRIPTEN OR WIN32 OR VITA)
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
if(ANDROID OR EMSCRIPTEN OR VITA)
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
elseif(VITA)
  message(STATUS "FFmpeg disabled on Vita -- video/av features unavailable until ported")
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
elseif(VITA)
  # Vita has no RtAudio backend -- audio goes through SDL3's audio subsystem instead
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
