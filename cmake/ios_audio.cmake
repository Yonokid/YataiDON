# SDL's AudioQueue backend queues at least twice its 15 ms minimum even when
# the app requests 128 frames. Keep the change confined to this iOS target.
# Investigation and device measurements: platform/ios/LATENCY_FIX.md.
set(IOS_AUDIO_QUEUE_MIN_MS "4" CACHE STRING "iOS playback queue half-duration target in ms (15 restores SDL default)")
if(NOT IOS_AUDIO_QUEUE_MIN_MS MATCHES "^[0-9]+$" OR
   IOS_AUDIO_QUEUE_MIN_MS LESS 1 OR IOS_AUDIO_QUEUE_MIN_MS GREATER 40)
  message(FATAL_ERROR "IOS_AUDIO_QUEUE_MIN_MS must be an integer from 1 to 40")
endif()
if(NOT SDL3_SOURCE_DIR)
  set(SDL3_SOURCE_DIR "${sdl3_SOURCE_DIR}")
endif()
set(_ios_coreaudio "${SDL3_SOURCE_DIR}/src/audio/coreaudio/SDL_coreaudio.m")
if(NOT EXISTS "${_ios_coreaudio}")
  message(FATAL_ERROR "Cannot locate SDL_coreaudio.m (SDL3_SOURCE_DIR='${SDL3_SOURCE_DIR}'); the iOS latency patch needs a vendored SDL checkout")
endif()
file(READ "${_ios_coreaudio}" _ios_audio_source)
set(_ios_audio_anchor "    device->hidden->numAudioBuffers = numAudioBuffers;")
string(FIND "${_ios_audio_source}" "${_ios_audio_anchor}" _ios_audio_end)
if(_ios_audio_end EQUAL -1)
  message(FATAL_ERROR "SDL AudioQueue implementation changed; review the iOS latency patch")
endif()
string(FIND "${_ios_audio_source}" "${_ios_audio_anchor}" _ios_audio_end_last REVERSE)
if(NOT _ios_audio_end_last EQUAL _ios_audio_end)
  message(FATAL_ERROR "iOS latency patch anchor appears more than once in SDL_coreaudio.m; review the patch")
endif()
# Replace an existing patch as well as applying to fresh sources. This removes
# instrumentation left in dependency checkouts by earlier development builds.
set(_ios_audio_marker "#if defined(SDL_PLATFORM_IOS) && defined(YATAIDON_IOS_AUDIO_QUEUE_MIN_MS)")
string(FIND "${_ios_audio_source}" "${_ios_audio_marker}" _ios_audio_start)
if(_ios_audio_start EQUAL -1)
  set(_ios_audio_start ${_ios_audio_end})
elseif(_ios_audio_start GREATER _ios_audio_end)
  message(FATAL_ERROR "Unexpected SDL iOS audio patch layout")
else()
  string(FIND "${_ios_audio_source}" "${_ios_audio_marker}" _ios_audio_start_last REVERSE)
  if(NOT _ios_audio_start_last EQUAL _ios_audio_start)
    message(FATAL_ERROR "iOS latency patch marker appears more than once in SDL_coreaudio.m; review the patch")
  endif()
endif()
set(_ios_audio_replacement [=[#if defined(SDL_PLATFORM_IOS) && defined(YATAIDON_IOS_AUDIO_QUEUE_MIN_MS)
    if (!device->recording) {
        numAudioBuffers = (msecs < YATAIDON_IOS_AUDIO_QUEUE_MIN_MS)
            ? (int)SDL_ceil(YATAIDON_IOS_AUDIO_QUEUE_MIN_MS / msecs) * 2 : 3;
    }
#endif
]=])
string(SUBSTRING "${_ios_audio_source}" 0 ${_ios_audio_start} _ios_audio_prefix)
string(SUBSTRING "${_ios_audio_source}" ${_ios_audio_end} -1 _ios_audio_suffix)
set(_ios_audio_patched "${_ios_audio_prefix}${_ios_audio_replacement}${_ios_audio_suffix}")
if(NOT _ios_audio_source STREQUAL _ios_audio_patched)
  file(WRITE "${_ios_coreaudio}" "${_ios_audio_patched}")
endif()
target_compile_definitions(SDL3-static PRIVATE
  YATAIDON_IOS_AUDIO_QUEUE_MIN_MS=${IOS_AUDIO_QUEUE_MIN_MS}.0)
