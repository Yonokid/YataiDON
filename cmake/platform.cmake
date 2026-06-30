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
        bcrypt
        secur32
        ws2_32
        ucrtbase
        dbghelp
    )
  if(TARGET SDL3::SDL3-static)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)
  elseif(TARGET SDL3::SDL3)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
  endif()
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
        "-framework VideoToolbox"
        "-framework CoreFoundation"
        "-framework CoreMedia"
    )
  if(TARGET SDL3::SDL3-static)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)
  elseif(TARGET SDL3::SDL3)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
  endif()
elseif(ANDROID)
  target_link_libraries(${PROJECT_NAME} PRIVATE
        log
        android
        EGL
        GLESv3
        z
        aaudio
        OpenSLES
        vorbis
        vorbisfile
        ogg
    )
  if(TARGET SDL3::SDL3-static)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)
  elseif(TARGET SDL3::SDL3)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
  endif()
  target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_ANDROID)
  target_link_options(${PROJECT_NAME} PRIVATE -Wl,-z,max-page-size=16384)
elseif(EMSCRIPTEN)
  set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
  target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_WEB)
  if(TARGET SDL3::SDL3-static)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)
  elseif(TARGET SDL3::SDL3)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
  endif()
  target_link_options(${PROJECT_NAME} PRIVATE
    -sUSE_SDL=3
    -sMAX_WEBGL_VERSION=2
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=134217728
    -sASSERTIONS=1
    -sNO_DISABLE_EXCEPTION_CATCHING
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/config.toml@/dev-config.toml"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/config.toml@/config.toml"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/shader@/shader"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/Songs@/Songs"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/Skins/PyTaikoGreen@/Skins/PyTaikoGreen"
    "SHELL:--exclude-file */Videos/*"
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
  if(TARGET SDL3::SDL3-static)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)
  elseif(TARGET SDL3::SDL3)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
  endif()
  if(EXISTS "/usr/bin/mold")
    target_link_options(${PROJECT_NAME} PRIVATE -fuse-ld=mold)
  endif()
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
  target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(ANDROID OR EMSCRIPTEN)
    target_compile_options(${PROJECT_NAME} PRIVATE
            -O0
            -g
            -fmax-errors=0
            -fno-omit-frame-pointer
        )
  elseif(NOT WIN32)
    target_compile_options(${PROJECT_NAME} PRIVATE
            -O0
            -g
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
            -O0
            -g
            -gdwarf-4
            -fmax-errors=0
            -fno-omit-frame-pointer
        )
  endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Profiling")
  target_compile_options(${PROJECT_NAME} PRIVATE
        -O2
        -g
        -fno-omit-frame-pointer
        -DNDEBUG
    )
  message(STATUS "Profiler build")
else()
  if(WIN32)
    target_compile_options(${PROJECT_NAME} PRIVATE
          -O2
          -march=x86-64
          -DNDEBUG
      )
  elseif(APPLE)
    target_compile_options(${PROJECT_NAME} PRIVATE
          -O2
          -DNDEBUG
          -flto=auto
      )
  elseif(ANDROID)
    target_compile_options(${PROJECT_NAME} PRIVATE
          -O2
          -DNDEBUG
      )
  elseif(EMSCRIPTEN)
    target_compile_options(${PROJECT_NAME} PRIVATE
          -O2
          -DNDEBUG
      )
  else()
    target_compile_options(${PROJECT_NAME} PRIVATE
          -O2
          -march=x86-64
          -DNDEBUG
          -flto=auto
      )
    target_link_options(${PROJECT_NAME} PRIVATE -flto=auto)
  endif()
endif()
