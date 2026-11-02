set(game_root "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(vendor "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
set(PS5_FFMPEG_PREFIX "" CACHE PATH "PS5 FFmpeg prefix (include and lib)")
if(NOT EXISTS "${PS5_FFMPEG_PREFIX}/lib/libavformat.a")
  message(FATAL_ERROR "Set PS5_FFMPEG_PREFIX to a PS5-built FFmpeg installation")
endif()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_CPACK OFF CACHE BOOL "" FORCE)
set(ENABLE_EXTERNAL_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_MPEG OFF CACHE BOOL "" FORCE)
set(LIBSAMPLERATE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBSAMPLERATE_INSTALL OFF CACHE BOOL "" FORCE)
set(CPU_CLIPS_POSITIVE FALSE CACHE BOOL "" FORCE)
set(CPU_CLIPS_NEGATIVE FALSE CACHE BOOL "" FORCE)
add_subdirectory(${vendor}/libsamplerate ${CMAKE_BINARY_DIR}/deps/samplerate)
add_subdirectory(${vendor}/libsndfile ${CMAKE_BINARY_DIR}/deps/sndfile)
add_subdirectory(${vendor}/miniz ${CMAKE_BINARY_DIR}/deps/miniz)
set(ENABLE_INTERNAL_QUEUE_H ON)
set(ENABLE_TESTS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(${vendor}/libebur128 ${CMAKE_BINARY_DIR}/deps/ebur128)
target_include_directories(ebur128 INTERFACE "${vendor}/libebur128/ebur128")

file(GLOB lua_sources ${vendor}/lua/src/*.c)
list(FILTER lua_sources EXCLUDE REGEX "/lua(c)?\\.c$")
add_library(yataidon_lua STATIC ${lua_sources})
target_include_directories(yataidon_lua PUBLIC ${vendor}/lua/src)
foreach(header lua.h lauxlib.h lualib.h lua.hpp luaconf.h)
  file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/generated/lua/${header}"
       CONTENT "#include \"${vendor}/lua/src/${header}\"\n")
endforeach()

add_library(yataidon_sqlite STATIC ${vendor}/sqlite/sqlite3.c)
target_include_directories(yataidon_sqlite PUBLIC ${vendor}/sqlite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(yataidon_sqlite PRIVATE SQLITE_THREADSAFE=1 SQLITE_CUSTOM_INCLUDE=sqlite-config.h)

file(GLOB_RECURSE game_sources CONFIGURE_DEPENDS ${game_root}/src/*.cpp)
list(FILTER game_sources EXCLUDE REGEX "/src/platform/")
list(FILTER game_sources EXCLUDE REGEX "/gfx_raylib\\.cpp$")
list(FILTER game_sources EXCLUDE REGEX "/src/libs/optional/")
add_library(yataidon_ps5_game STATIC ${game_sources}
    ${CMAKE_CURRENT_SOURCE_DIR}/native-file-data.c
  ${game_root}/src/platform/platform_ps5.cpp ${game_root}/src/platform/platform_ps5_audio.cpp
  ${game_root}/src/platform/platform_ps5_video.cpp
  ${game_root}/src/platform/platform_desktop.cpp)
target_include_directories(yataidon_ps5_game PUBLIC
  ${game_root}/src ${CMAKE_BINARY_DIR}/generated
  ${vendor}/rapidjson/include ${vendor}/tomlplusplus/include
  ${vendor}/spdlog/include ${vendor}/sol2/include ${RAYLIB_SOURCE_DIR}/src/external
  ${PS5_FFMPEG_PREFIX}/include)
target_compile_definitions(yataidon_ps5_game PUBLIC
  YATAIDON_PLATFORM_PS5 YATAIDON_USE_SDL2 SDL_MAIN_HANDLED
  YATAIDON_SNDFILE_PCM_ONLY # libsndfile without external codecs: FFmpeg decodes the rest
  SOL_ALL_SAFETIES_ON=1 YATAIDON_GFX_BGFX=1
  _LIBCPP_ENABLE_EXPERIMENTAL) # SDK libc++ (<20) hides std::stop_token behind this; header-only, no extra lib
target_compile_options(yataidon_ps5_game PRIVATE -O1 -ffunction-sections -fdata-sections)

# Link descriptions only: the native title imports the console's firmware
# decoder. These stub bodies are never packaged or executed.
add_library(yataidon_videodec_stub SHARED ${CMAKE_CURRENT_SOURCE_DIR}/videodec-link-stub.c)
set_target_properties(yataidon_videodec_stub PROPERTIES OUTPUT_NAME SceVideodec2 NO_SONAME TRUE LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_link_options(yataidon_videodec_stub PRIVATE -nostdlib LINKER:-soname,libSceVideodec2.prx)
target_link_libraries(yataidon_ps5_game PUBLIC
  bgfx bx SDL2-static yataidon_vulkan yataidon_lua yataidon_sqlite sndfile samplerate miniz ebur128 SceAudioOut SceSysmodule yataidon_videodec_stub yataidon_SceAgc_stub yataidon_SceAgcDriver_stub)
foreach(lib avformat avcodec swscale swresample avutil opus)
  target_link_libraries(yataidon_ps5_game PUBLIC ${PS5_FFMPEG_PREFIX}/lib/lib${lib}.a)
endforeach()
# The installed title is linked by package.py with the native-app ABI. A payload
# executable is not a valid validation target for this delivery format.
add_custom_target(YataiDON-PS5 DEPENDS yataidon_ps5_game
  yataidon_videodec_stub yataidon_SceAgc_stub yataidon_SceAgcDriver_stub)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/native-archives.txt CONTENT
"$<TARGET_FILE:yataidon_ps5_game>\n$<TARGET_FILE:bgfx>\n$<TARGET_FILE:bx>\n$<TARGET_FILE:bimg>\n$<TARGET_FILE:yataidon_vulkan>\n$<TARGET_FILE:yataidon_lua>\n$<TARGET_FILE:yataidon_sqlite>\n$<TARGET_FILE:sndfile>\n$<TARGET_FILE:samplerate>\n$<TARGET_FILE:miniz>\n$<TARGET_FILE:ebur128>\n$<TARGET_FILE:SDL2-static>\n")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/shaders/spirv")
file(GLOB shader_files CONFIGURE_DEPENDS "${game_root}/shader/bgfx/[fv]s_*.sc")
file(GLOB shader_includes CONFIGURE_DEPENDS "${game_root}/shader/bgfx/*.sh")
foreach(shader_file ${shader_files})
  get_filename_component(shader "${shader_file}" NAME_WE)
  if(shader MATCHES "^vs_")
    set(type vertex)
  else()
    set(type fragment)
  endif()
  set(output "${CMAKE_BINARY_DIR}/generated/shaders/spirv/${shader}.sc.bin.h")
  add_custom_command(OUTPUT "${output}"
    COMMAND "${YATAIDON_SHADERC}" -f "${shader_file}" -o "${output}"
      --type ${type} --platform linux -p spirv --bin2c ${shader}_spv
      --varyingdef "${game_root}/shader/bgfx/varying.def.sc" -i "${BGFX_SOURCE_DIR}/bgfx/src"
    DEPENDS "${shader_file}" ${shader_includes} "${game_root}/shader/bgfx/varying.def.sc" VERBATIM)
  target_sources(yataidon_ps5_game PRIVATE "${output}")
endforeach()
set(font_output "${CMAKE_BINARY_DIR}/generated/default_font.cpp")
add_custom_command(OUTPUT "${font_output}"
  COMMAND ${CMAKE_COMMAND} -DIN=${game_root}/src/libs/gfx/default.ttf -DOUT=${font_output} -DNAME=default_font_ttf_data -P ${game_root}/cmake/embed.cmake
  DEPENDS ${game_root}/src/libs/gfx/default.ttf ${game_root}/cmake/embed.cmake VERBATIM)
target_sources(yataidon_ps5_game PRIVATE "${font_output}")
