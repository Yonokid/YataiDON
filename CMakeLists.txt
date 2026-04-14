cmake_minimum_required(VERSION 3.14)
project(YataiDON VERSION 0.1.0 LANGUAGES C CXX)

cmake_policy(SET CMP0002 NEW)  # Logical target names must be globally unique
cmake_policy(SET CMP0079 NEW)  # Allow target_link_libraries() everywhere

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-dev")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Dependency management
include(FetchContent)

### Dependencies

# SDL3
if(NOT WIN32)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SQLITE3 QUIET sqlite3)
  endif()
endif()

option(FETCH_SDL3 "Fetch SDL3 via FetchContent instead of using system package" ON)

if(NOT FETCH_SDL3)
  find_package(SDL3 REQUIRED CONFIG)
  message(STATUS "Using system SDL3")
else()
  find_package(SDL3 QUIET CONFIG)
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
if(TARGET SDL3::SDL3-static AND NOT TARGET SDL3::SDL3)
  add_library(SDL3::SDL3 ALIAS SDL3::SDL3-static)
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
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG        5.5
  GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(raylib)
if(NOT raylib_POPULATED)
  FetchContent_Populate(raylib)
  set(_raylib_lib_cfg "${raylib_SOURCE_DIR}/cmake/LibraryConfigurations.cmake")
  file(READ "${_raylib_lib_cfg}" _cfg)
  string(REPLACE "find_package(SDL2 REQUIRED)" "find_package(SDL3 REQUIRED CONFIG)" _cfg "${_cfg}")
  string(REPLACE "set(LIBS_PRIVATE SDL2::SDL2)" "set(LIBS_PRIVATE SDL3::SDL3)" _cfg "${_cfg}")
  file(WRITE "${_raylib_lib_cfg}" "${_cfg}")
  add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR} EXCLUDE_FROM_ALL)
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
    GIT_TAG v1.13.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spdlog)

# Lua
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/marovira/lua.git
    GIT_TAG 5.4.8
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(lua)

# Sol2
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.5.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(sol2)

# libsndfile
if(WIN32)
  message(STATUS "Using local libsndfile on Windows")
  set(SNDFILE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src/libs/audio")
  set(SNDFILE_LIBRARY "${CMAKE_SOURCE_DIR}/src/libs/audio/libsndfile.a")

  if(NOT EXISTS ${SNDFILE_LIBRARY})
    message(FATAL_ERROR "libsndfile not found: ${SNDFILE_LIBRARY}")
  endif()

  add_library(sndfile STATIC IMPORTED)
  set_target_properties(sndfile PROPERTIES
        IMPORTED_LOCATION ${SNDFILE_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${SNDFILE_INCLUDE_DIR}
    )
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
    GIT_TAG 1.2.2
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(libsndfile)
endif()

# libsamplerate
if(WIN32)
  message(STATUS "Using local libsamplerate on Windows")
  set(SAMPLERATE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src/libs/audio")
  set(SAMPLERATE_LIBRARY "${CMAKE_SOURCE_DIR}/src/libs/audio/libsamplerate.a")

  if(NOT EXISTS ${SAMPLERATE_LIBRARY})
    message(FATAL_ERROR "libsamplerate not found: ${SAMPLERATE_LIBRARY}")
  endif()

  add_library(samplerate STATIC IMPORTED)
  set_target_properties(samplerate PROPERTIES
        IMPORTED_LOCATION ${SAMPLERATE_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${SAMPLERATE_INCLUDE_DIR}
    )
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
            GIT_TAG 0.2.2
            GIT_SHALLOW TRUE
        )
    FetchContent_MakeAvailable(libsamplerate)
  endif()
endif()

# FFmpeg (libavformat, libavcodec, libavutil, libswscale)
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

# digestpp
FetchContent_Declare(
    digestpp
    GIT_REPOSITORY https://github.com/kerukuro/digestpp.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(digestpp)

### PortAudio (local prebuilt library)
# Determine platform-specific library name
if(WIN32)
  set(PORTAUDIO_LIB_NAME "libportaudio-win.a")
elseif(APPLE)
  set(PORTAUDIO_LIB_NAME "libportaudio-macos.a")
elseif(UNIX)
  set(PORTAUDIO_LIB_NAME "libportaudio-linux.a")
endif()

set(PORTAUDIO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src/libs/audio")
set(PORTAUDIO_LIBRARY "${CMAKE_SOURCE_DIR}/src/libs/audio/${PORTAUDIO_LIB_NAME}")

# Check if PortAudio library exists
if(NOT EXISTS ${PORTAUDIO_LIBRARY})
  message(FATAL_ERROR "PortAudio library not found: ${PORTAUDIO_LIBRARY}")
endif()

# Create imported library target for PortAudio
add_library(portaudio STATIC IMPORTED)
set_target_properties(portaudio PROPERTIES
    IMPORTED_LOCATION ${PORTAUDIO_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${PORTAUDIO_INCLUDE_DIR}
)

### Source files

file(GLOB_RECURSE CppFiles src/*.cpp)
file(GLOB_RECURSE HeaderFiles
    src/*.h
    src/*.hpp
)
message(STATUS "C++ files found: ${CppFiles}")

add_executable(${PROJECT_NAME} ${CppFiles} ${HeaderFiles})

if(WIN32)
  foreach(_lib avformat avcodec avutil swscale swresample)
    file(GLOB _dll "${FFMPEG_LIB_DIR}/../bin/${_lib}*.dll")
    if(_dll)
      add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${_dll}
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            )
    else()
      message(WARNING "FFmpeg DLL not found for ${_lib}")
    endif()
  endforeach()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/libs/audio
    ${PORTAUDIO_INCLUDE_DIR}
    ${rapidjson_SOURCE_DIR}/include
    ${tomlplusplus_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    sndfile
    samplerate
    portaudio
    digestpp
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    lua::lua
    sol2::sol2
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::swscale
    FFmpeg::avutil
    FFmpeg::swresample
    SQLite3_lib
)

# Platform specific (Thanks GPT)

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE
        opengl32
        gdi32
        winmm
        setupapi
        ole32
        vorbisenc
        vorbis
        FLAC
        opus
        ogg
        mp3lame
        mpg123
        shlwapi
        # FFmpeg on Windows also needs these at the end of the link line
        bcrypt
        secur32
        ws2_32
    )
  if(TARGET SDL3::SDL3-static)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)
  elseif(TARGET SDL3::SDL3)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
  endif()
  # Static linking on Windows
  target_link_options(${PROJECT_NAME} PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
  set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "-Wl,--allow-multiple-definition"
    )
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework CoreVideo"
        "-framework IOKit"
        "-framework Cocoa"
        "-framework GLUT"
        "-framework OpenGL"
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        # FFmpeg on macOS may need VideoToolbox for hardware-accelerated decode
        "-framework VideoToolbox"
        "-framework CoreFoundation"
        "-framework CoreMedia"
    )
elseif(UNIX)
  target_link_libraries(${PROJECT_NAME} PRIVATE
        GL
        m
        pthread
        dl
        rt
        X11
        asound
    )
  # Explicit SDL3 link so input.cpp can include <SDL3/SDL.h> and call the gamepad
  # API directly (bypassing raylib's broken SDL3 gamepad backend).
  # Target name differs: fetched SDL3 builds SDL3::SDL3-static; system installs
  # typically provide SDL3::SDL3 (shared).
  if(TARGET SDL3::SDL3-static)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)
  elseif(TARGET SDL3::SDL3)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
  endif()
  # Set rpath for audio library
  set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "${CMAKE_SOURCE_DIR}/src/libs/audio"
        INSTALL_RPATH "${CMAKE_SOURCE_DIR}/src/libs/audio"
    )
  find_library(JACK_LIB jack)
  if(JACK_LIB)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${JACK_LIB})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_JACK)
  endif()
  find_library(PULSE_LIB pulse)
  if(PULSE_LIB)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${PULSE_LIB})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_PULSE)
  endif()
  target_compile_definitions(${PROJECT_NAME} PRIVATE
      SDL_MAIN_HANDLED
  )
endif()

# AddressSanitizer for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(NOT WIN32)
    target_compile_options(${PROJECT_NAME} PRIVATE
            -O0  # No optimization for debug
            -g   # Debug symbols
            -fmax-errors=0
            -fsanitize=address
            -fsanitize=leak
            -fsanitize=undefined
            -fno-omit-frame-pointer
        )
  else()
    target_compile_options(${PROJECT_NAME} PRIVATE
            -O0  # No optimization for debug
            -g   # Debug symbols
            -fmax-errors=0
            -fno-omit-frame-pointer
        )
  endif()
  if(NOT WIN32)
    target_link_options(${PROJECT_NAME} PRIVATE
            -fsanitize=address
            -fsanitize=leak
            -fsanitize=undefined
        )
    message(STATUS "AddressSanitizer enabled for Debug build")
  endif()
else()
  target_compile_options(${PROJECT_NAME} PRIVATE
        -O2
        -march=x86-64
        -DNDEBUG  # Disable assertions
        -flto  # Link-time optimization
    )
  target_link_options(${PROJECT_NAME} PRIVATE
        -flto  # Link-time optimization
        -s  # Strip symbols
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_target(run
    COMMAND ${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "Running ${PROJECT_NAME}..."
)

message(STATUS "")
message(STATUS "===============================================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "PortAudio Library: ${PORTAUDIO_LIBRARY}")
message(STATUS "SDL3: ${SDL3_VERSION}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32)
  message(STATUS "Sanitizers: AddressSanitizer, LeakSanitizer, UndefinedBehaviorSanitizer")
endif()
message(STATUS "===============================================================")
message(STATUS "")
