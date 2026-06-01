# Sqlite3
if(NOT WIN32)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SQLITE3 QUIET sqlite3)
  endif()
endif()

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
      GIT_TAG master
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

# PortAudio (local prebuilt library)
if(WIN32)
  set(PORTAUDIO_LIB_NAME "libportaudio-win.a")
elseif(APPLE)
  set(PORTAUDIO_LIB_NAME "libportaudio-macos.a")
elseif(UNIX)
  set(PORTAUDIO_LIB_NAME "libportaudio-linux.a")
endif()

set(PORTAUDIO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src/libs/audio")
set(PORTAUDIO_LIBRARY "${CMAKE_SOURCE_DIR}/src/libs/audio/${PORTAUDIO_LIB_NAME}")

if(NOT EXISTS ${PORTAUDIO_LIBRARY})
  message(FATAL_ERROR "PortAudio library not found: ${PORTAUDIO_LIBRARY}")
endif()

add_library(portaudio STATIC IMPORTED)
set_target_properties(portaudio PROPERTIES
    IMPORTED_LOCATION ${PORTAUDIO_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${PORTAUDIO_INCLUDE_DIR}
)
