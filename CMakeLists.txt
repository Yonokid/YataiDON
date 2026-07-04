cmake_minimum_required(VERSION 3.14)
project(YataiDON VERSION 1.0.0 LANGUAGES C CXX)

cmake_policy(SET CMP0002 NEW)  # Logical target names must be globally unique
cmake_policy(SET CMP0079 NEW)  # Allow target_link_libraries() everywhere

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE INTERNAL "")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.cmake-deps" CACHE PATH "" FORCE)

include(cmake/deps.cmake)
include(cmake/codegen.cmake)

### Source files

# miniz (vendored single-file ZIP library for .osz extraction)
add_library(miniz STATIC src/libs/miniz/miniz.c)
target_include_directories(miniz PUBLIC src/libs/miniz)

file(GLOB_RECURSE CppFiles src/*.cpp)
file(GLOB_RECURSE HeaderFiles
    src/*.h
    src/*.hpp
)
message(STATUS "C++ files found: ${CppFiles}")

if(ANDROID)
  add_library(${PROJECT_NAME} SHARED ${CppFiles} ${HeaderFiles})
else()
  add_executable(${PROJECT_NAME} ${CppFiles} ${HeaderFiles})
endif()
add_dependencies(${PROJECT_NAME} skin_config_gen texture_ids_gen)

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

  # MinGW runtime + libsamplerate are not fully statically linked (MSYS2 ships
  # libsamplerate as a shared-only package), so copy their DLLs next to the
  # executable or the game fails to launch with a missing-DLL error.
  get_filename_component(_mingw_bin_dir ${CMAKE_CXX_COMPILER} DIRECTORY)
  foreach(_rtdll libstdc++-6.dll libwinpthread-1.dll libgcc_s_seh-1.dll libsamplerate-0.dll)
    if(EXISTS "${_mingw_bin_dir}/${_rtdll}")
      add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_mingw_bin_dir}/${_rtdll}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            )
    else()
      message(WARNING "Runtime DLL not found: ${_rtdll}")
    endif()
  endforeach()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${raylib_SOURCE_DIR}/src/external
    ${rapidjson_SOURCE_DIR}/include
    ${tomlplusplus_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    lua::lua
    sol2::sol2
    SQLite3_lib
    miniz
)

if(NOT ANDROID AND NOT EMSCRIPTEN AND NOT VITA)
  target_link_libraries(${PROJECT_NAME} PRIVATE cpptrace::cpptrace)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::swscale
    FFmpeg::avutil
    FFmpeg::swresample
)

target_link_libraries(${PROJECT_NAME} PRIVATE sndfile samplerate)
if(NOT ANDROID AND NOT EMSCRIPTEN AND NOT VITA)
  target_link_libraries(${PROJECT_NAME} PRIVATE rtaudio)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE portaudio)

include(cmake/platform.cmake)

if(NOT VITA)
  # vita_create_self (below) hardcodes ${CMAKE_CURRENT_BINARY_DIR}/${source}
  # with no way to point it at a different directory, so Vita's raw ELF has
  # to stay at the build dir root instead of being relocated to bin/.
  set_target_properties(${PROJECT_NAME} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
endif()

add_custom_target(run
    COMMAND ${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "Running ${PROJECT_NAME}..."
)

if(VITA)
  # vita.toolchain.cmake does not auto-include this -- it only ships the
  # toolchain file itself, so vita_create_self/vita_create_vpk need it pulled
  # in explicitly.
  include("${VITASDK}/share/vita.cmake")

  vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})

  # Bundle the shader assets, config.toml, and the default PyTaikoGreen skin
  # read-only into the vpk (app0:). Everything in this codebase reads these
  # via a cwd-relative path, and cwd is ux0:data/YataiDON (writable, for
  # scores.db/Songs/user-added skins) -- a different filesystem root from
  # app0:, so src/libs/filesystem.cpp seeds config.toml/Skins/PyTaikoGreen
  # from app0: into ux0:data/YataiDON on first run. Songs are NOT bundled --
  # those are user-provided beatmaps that belong on ux0: from the start.
  file(GLOB_RECURSE _vita_shader_files
    RELATIVE "${CMAKE_SOURCE_DIR}/shader/es"
    "${CMAKE_SOURCE_DIR}/shader/es/*"
  )
  file(GLOB_RECURSE _vita_skin_files
    RELATIVE "${CMAKE_SOURCE_DIR}/Skins/PyTaikoGreen"
    "${CMAKE_SOURCE_DIR}/Skins/PyTaikoGreen/*"
  )
  set(_vita_vpk_files "")
  foreach(_f ${_vita_shader_files})
    list(APPEND _vita_vpk_files "${CMAKE_SOURCE_DIR}/shader/es/${_f}" "shader/es/${_f}")
  endforeach()
  foreach(_f ${_vita_skin_files})
    list(APPEND _vita_vpk_files "${CMAKE_SOURCE_DIR}/Skins/PyTaikoGreen/${_f}" "Skins/PyTaikoGreen/${_f}")
  endforeach()

  # PVR_PSP2's runtime .suprx modules (deps.cmake) -- Vita3K looks for these
  # at app0:module/<name>.suprx, i.e. bundled inside our own vpk, not
  # installed system-wide like Piglet's taiHEN plugin was.
  file(GLOB _vita_pvr_modules "${pvr_psp2_runtime_SOURCE_DIR}/*.suprx")
  foreach(_f ${_vita_pvr_modules})
    get_filename_component(_fname "${_f}" NAME)
    list(APPEND _vita_vpk_files "${_f}" "module/${_fname}")
  endforeach()

  vita_create_vpk(${PROJECT_NAME}.vpk "YATA00001" ${PROJECT_NAME}.self
    VERSION "01.00"
    NAME "YataiDON"
    FILE ${CMAKE_SOURCE_DIR}/config.toml config.toml
    FILE ${_vita_vpk_files}
  )
endif()

message(STATUS "")
message(STATUS "===============================================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32 AND NOT ANDROID AND NOT VITA)
  message(STATUS "Sanitizers: AddressSanitizer, LeakSanitizer, UndefinedBehaviorSanitizer")
endif()
message(STATUS "===============================================================")
message(STATUS "")
