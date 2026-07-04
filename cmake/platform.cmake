if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE
        opengl32
        gdi32
        winmm
        setupapi
        ole32
        avrt
        ksuser
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
elseif(VITA)
  # SDL3-static and the Sce/PVR stub libs reference symbols in each other
  # (SDL3 needs SceGxm_stub/SceAppMgr_stub; the PVR video driver needs
  # gpu_es4_ext/IMGEGL), so a single left-to-right link order can never
  # satisfy everyone -- whichever consumer is listed first still won't see
  # symbols provided by something listed after it. --start-group/--end-group
  # makes ld keep re-scanning the whole set until nothing new resolves,
  # instead of depending on getting the order right by hand.
  set(_vita_sdl3_target "")
  if(TARGET SDL3::SDL3-static)
    set(_vita_sdl3_target SDL3::SDL3-static)
  elseif(TARGET SDL3::SDL3)
    set(_vita_sdl3_target SDL3::SDL3)
  endif()

  target_link_libraries(${PROJECT_NAME} PRIVATE
        -Wl,--start-group
        ${_vita_sdl3_target}
        # Stub libs SDL3-static links PRIVATE (see SDL's CMakeLists.txt
        # sdl_link_dependency(base ...)); static-lib PRIVATE deps don't
        # propagate, so relist them here -- same reason the ANDROID branch
        # above relists log/android/EGL/GLESv3 by hand.
        SceGxm_stub
        SceDisplay_stub
        SceCtrl_stub
        SceAppMgr_stub
        SceAppUtil_stub
        SceAudio_stub
        SceAudioIn_stub
        SceSysmodule_stub
        SceIofilemgr_stub
        SceCommonDialog_stub
        SceTouch_stub
        SceHid_stub
        SceMotion_stub
        ScePower_stub
        SceProcessmgr_stub
        SceCamera_stub
        SceIme_stub
        # VIDEO_VITA_PVR path (deps.cmake) -- PVR_PSP2 (github.com/GrapheneCt/
        # PVR_PSP2), vendored via FetchContent since it has no vitasdk
        # package; its actual runtime .suprx modules still need installing
        # to ur0:data/external (or wherever) on the target device/emulator,
        # same as any other Vita GPU driver homebrew depends on.
        libgpu_es4_ext_stub_weak
        libIMGEGL_stub_weak
        libGLESv2_stub_weak
        libGLESv1_CM_stub_weak
        -Wl,--end-group
    )
  target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_VITA)
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
  if(ANDROID OR EMSCRIPTEN OR VITA)
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
  elseif(VITA)
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
