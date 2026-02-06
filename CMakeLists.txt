cmake_minimum_required(VERSION 3.14)
project(YataiDON VERSION 0.1.0 LANGUAGES C CXX)

cmake_policy(SET CMP0002 NEW)  # Logical target names must be globally unique
cmake_policy(SET CMP0079 NEW)  # Allow target_link_libraries() in different directories

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Dependency management
include(FetchContent)

### Dependencies

# SDL2
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET sdl2)
endif()

if(SDL2_FOUND)
    message(STATUS "Using system SDL2")
    add_library(SDL2::SDL2-static INTERFACE IMPORTED)
    target_include_directories(SDL2::SDL2-static INTERFACE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(SDL2::SDL2-static INTERFACE ${SDL2_LIBRARIES})
    target_link_directories(SDL2::SDL2-static INTERFACE ${SDL2_LIBRARY_DIRS})

    # SDL2main is usually part of the same package on MSYS2
    add_library(SDL2::SDL2main INTERFACE IMPORTED)
    target_link_libraries(SDL2::SDL2main INTERFACE SDL2::SDL2-static)
else()
    message(STATUS "System SDL2 not found, fetching from source")
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL2)
endif()

# raylib
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(PLATFORM "SDL" CACHE STRING "" FORCE)
set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)
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
    GIT_TAG v1.13.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spdlog)

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
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SNDFILE QUIET sndfile)
    endif()

    if(SNDFILE_FOUND)
        message(STATUS "Using system libsndfile")
        add_library(sndfile INTERFACE IMPORTED)
        target_include_directories(sndfile INTERFACE ${SNDFILE_INCLUDE_DIRS})
        target_link_libraries(sndfile INTERFACE ${SNDFILE_LINK_LIBRARIES})
        target_link_directories(sndfile INTERFACE ${SNDFILE_LIBRARY_DIRS})
    else()
        message(STATUS "System libsndfile not found, fetching from source")
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

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/libs/audio
    ${PORTAUDIO_INCLUDE_DIR}
    ${rapidjson_SOURCE_DIR}/include
    ${tomlplusplus_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    SDL2::SDL2-static
    SDL2::SDL2main
    sndfile
    samplerate
    portaudio
    digestpp
    tomlplusplus::tomlplusplus
    spdlog::spdlog
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
    )
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
endif()

# AddressSanitizer for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -O0  # No optimization for debug
        -g   # Debug symbols
        -fmax-errors=0
        -fsanitize=address
        -fsanitize=leak
        -fsanitize=undefined
        -fno-omit-frame-pointer
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -fsanitize=address
        -fsanitize=leak
        -fsanitize=undefined
    )
    message(STATUS "AddressSanitizer enabled for Debug build")
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -O0  # No optimization for debug
        -g   # Debug symbols
        -fmax-errors=0
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
message(STATUS "SDL2: ${SDL2_VERSION}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Sanitizers: AddressSanitizer, LeakSanitizer, UndefinedBehaviorSanitizer")
endif()
message(STATUS "===============================================================")
message(STATUS "")
