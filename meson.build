project('YataiDON',
  ['c', 'cpp'],
  version: '0.1.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=2',
    'buildtype=debug',
  ],
  license : 'GPL-3.0'
)

cpp = meson.get_compiler('cpp')
host_system = host_machine.system()
is_msvc = cpp.get_id() == 'msvc'
is_gcc = cpp.get_id() == 'gcc'
is_clang = cpp.get_id() == 'clang'

sanitizers = get_option('buildtype') == 'debug' and not (host_system == 'windows' and is_gcc)

if is_msvc
  # MSVC-specific flags
  common_args = [
    '/W3',  # Warning level 3
    '/EHsc',  # Exception handling
    '/permissive-',  # Standards conformance
  ]
  debug_args = [
    '/Od',  # No optimization
    '/Zi',  # Debug info
  ]
else
  # GCC/Clang flags
  common_args = [
    '-fmax-errors=0',
  ]
  debug_args = [
    '-O0',
    '-g',
  ]
endif

sanitizer_args = []
sanitizer_link_args = []
if sanitizers
  if is_msvc
    # MSVC AddressSanitizer
    sanitizer_args = ['/fsanitize=address']
    sanitizer_link_args = []  # MSVC doesn't need link flags for ASan
    message('AddressSanitizer enabled for Debug build (MSVC)')
  elif is_clang or is_gcc
    # GCC/Clang sanitizers
    sanitizer_args = [
      '-fsanitize=address',
      '-fsanitize=undefined',
      '-fno-omit-frame-pointer',
    ]
    # Only add leak sanitizer on non-Windows or if not GCC on Windows
    if not (host_system == 'windows' and is_gcc)
      if not host_system == 'windows'
        sanitizer_args += ['-fsanitize=leak']
      endif
    endif
    sanitizer_link_args = sanitizer_args
    if host_system == 'windows' and is_gcc
      message('AddressSanitizer not available with GCC/MinGW on Windows')
    else
      message('AddressSanitizer enabled for Debug build')
    endif
  endif
endif

# ────────────────────────────────────────────────────────────────
# Dependencies
# ────────────────────────────────────────────────────────────────

# SDL2
sdl2_dep = dependency('sdl2', 
  version: '>=2.0.0',
  static: true
)

# raylib

# raylib_dep = dependency('raylib',
#   version: '>=5.0',
#   static: true,
# )

# https://github.com/gilzoide/raylib-meson-template/blob/main/meson.build
cmake = import('cmake')
raylib_opts = cmake.subproject_options()
raylib_opts.set_install(false)
raylib_opts.add_cmake_defines({
  'BUILD_EXAMPLES': 'OFF',
  'BUILD_GAMES': 'OFF',
  'PLATFORM': 'SDL',
  'CUSTOMIZE_BUILD': 'ON',
  'SUPPORT_MODULE_RAUDIO': 'OFF',
  'SUPPORT_CUSTOM_FRAME_CONTROL': 'ON'
})
raylib_subproject = cmake.subproject('raylib', options: raylib_opts)
raylib_dep = raylib_subproject.dependency('raylib')

# RapidJSON (header-only)
rapidjson_dep = dependency('RapidJSON',
  required: false
)
if not rapidjson_dep.found()
  rapidjson_paths = []
  if host_system == 'windows'
    rapidjson_paths = [
      'C:/msys64/mingw64/include',
      '/mingw64/include',
    ]
  else
    rapidjson_paths = ['/usr/include', '/usr/local/include']
  endif
  
  rapidjson_found = false
  foreach path : rapidjson_paths
    if import('fs').is_dir(path / 'rapidjson')
      rapidjson_dep = declare_dependency(
        include_directories: include_directories(path, is_system: true)
      )
      rapidjson_found = true
      message('Found rapidjson headers at: ' + path)
      break
    endif
  endforeach
  
  if not rapidjson_found
    error('RapidJSON headers not found. Please install: mingw-w64-x86_64-rapidjson (MSYS2) or rapidjson-dev (Linux)')
  endif
endif

# tomlplusplus
tomlplusplus_dep = dependency('tomlplusplus',
  version: '>=3.0.0',
  required: false,
  static: true
)

if not tomlplusplus_dep.found()
  # Try to find the compiled library manually
  tomlplusplus_lib = cpp.find_library('tomlplusplus', required: false, static: true)
  
  if tomlplusplus_lib.found()
    toml_paths = []
    if host_system == 'windows'
      toml_paths = [
        'C:/msys64/mingw64/include',
        '/mingw64/include',
      ]
    else
      toml_paths = ['/usr/local/include', '/usr/include']
    endif
    
    toml_found = false
    foreach path : toml_paths
      if import('fs').is_dir(path / 'toml++') or import('fs').exists(path / 'toml.hpp')
        tomlplusplus_dep = declare_dependency(
          include_directories: include_directories(path, is_system: true),
          dependencies: tomlplusplus_lib
        )
        toml_found = true
        message('Found tomlplusplus library and headers at: ' + path)
        break
      endif
    endforeach
    
    if not toml_found
      error('tomlplusplus headers not found')
    endif
  else
    # Fallback to header-only mode
    message('tomlplusplus library not found, using header-only mode')
    toml_paths = []
    if host_system == 'windows'
      toml_paths = [
        'C:/msys64/mingw64/include',
        '/mingw64/include',
      ]
    else
      toml_paths = ['/usr/local/include', '/usr/include']
    endif
    
    toml_found = false
    foreach path : toml_paths
      if import('fs').is_dir(path / 'toml++') or import('fs').exists(path / 'toml.hpp')
        tomlplusplus_dep = declare_dependency(
          include_directories: include_directories(path, is_system: true),
          compile_args: ['-DTOML_HEADER_ONLY=1']
        )
        toml_found = true
        message('Found tomlplusplus headers at: ' + path)
        break
      endif
    endforeach
    
    if not toml_found
      error('tomlplusplus not found. Please install: mingw-w64-x86_64-tomlplusplus (MSYS2) or run the installation script')
    endif
  endif
endif

# spdlog
spdlog_dep = dependency('spdlog',
  version: '>=1.0.0',
  static: true
)

# digestpp
digestpp_paths = []
if host_system == 'windows'
  digestpp_paths = [
    'C:/msys64/mingw64/include',
    '/mingw64/include',
  ]
else
  digestpp_paths = ['/usr/local/include', '/usr/include']
endif

digestpp_found = false
foreach path : digestpp_paths
  if import('fs').is_dir(path / 'digestpp')
    digestpp_dep = declare_dependency(
      include_directories: include_directories(path, is_system: true)
    )
    digestpp_found = true
    message('Found digestpp headers at: ' + path)
    break
  endif
endforeach

if not digestpp_found
  error('digestpp headers not found. Please run the installation script or manually install from https://github.com/kerukuro/digestpp')
endif

# libsndfile
sndfile_dep = dependency('sndfile',
  version: '>=1.0.0',
  required: host_system != 'windows',
  static: true
)

if host_system == 'windows' and not sndfile_dep.found()
  if not sndfile_dep.found()
    message('Using local libsndfile on Windows')
    sndfile_lib = meson.current_source_dir() / 'src/libs/audio/libsndfile.a'
    if not import('fs').exists(sndfile_lib)
      error('libsndfile not found: ' + sndfile_lib)
    endif
    
    sndfile_dep = declare_dependency(
      dependencies: cpp.find_library('sndfile',
        dirs: meson.current_source_dir() / 'src/libs/audio',
        static: true
      ),
      include_directories: include_directories('src/libs/audio')
    )
  endif
endif

# libsamplerate
samplerate_dep = dependency('samplerate',
  version: '>=0.1.0',
  required: host_system != 'windows',
  static: true
)

if host_system == 'windows' and not samplerate_dep.found()
  if not samplerate_dep.found()
    message('Using local libsamplerate on Windows')
    samplerate_lib = meson.current_source_dir() / 'src/libs/audio/libsamplerate.a'
    if not import('fs').exists(samplerate_lib)
      error('libsamplerate not found: ' + samplerate_lib)
    endif
    
    samplerate_dep = declare_dependency(
      dependencies: cpp.find_library('samplerate',
        dirs: meson.current_source_dir() / 'src/libs/audio',
        static: true
      ),
      include_directories: include_directories('src/libs/audio')
    )
  endif
endif

# PortAudio (prebuilt library)
portaudio_lib_name = ''
if host_system == 'windows'
  portaudio_lib_name = 'libportaudio-win.a'
elif host_system == 'darwin'
  portaudio_lib_name = 'libportaudio-macos.a'
elif host_system == 'linux'
  portaudio_lib_name = 'libportaudio-linux.a'
else
  error('Unsupported platform for PortAudio: ' + host_system)
endif

portaudio_lib_path = meson.current_source_dir() / 'src/libs/audio' / portaudio_lib_name
if not import('fs').exists(portaudio_lib_path)
  error('PortAudio library not found: ' + portaudio_lib_path)
endif

portaudio_dep = declare_dependency(
  dependencies: cpp.find_library('portaudio',
    dirs: meson.current_source_dir() / 'src/libs/audio',
    static: true
  ),
  include_directories: include_directories('src/libs/audio')
)

# ────────────────────────────────────────────────
#  Platform-specific deps
# ────────────────────────────────────────────────

platform_deps = []
platform_link_args = []

if host_system == 'windows'
  if is_msvc
    platform_deps += [
      cpp.find_library('opengl32'),
      cpp.find_library('winmm'),
      cpp.find_library('setupapi'),
    ]
  else
    # MinGW/GCC
    platform_deps += [
      cpp.find_library('opengl32'),
      cpp.find_library('gdi32'),
      cpp.find_library('winmm'),
      cpp.find_library('setupapi'),
      cpp.find_library('ole32'),
      cpp.find_library('imm32'),
      cpp.find_library('version'),
      cpp.find_library('vorbisenc'),
      cpp.find_library('vorbis'),
      cpp.find_library('FLAC'),
      cpp.find_library('opus'),
      cpp.find_library('ogg'),
      cpp.find_library('mp3lame'),
      cpp.find_library('mpg123'),
      cpp.find_library('shlwapi'),
    ]
    platform_link_args += [
      '-static',
      '-static-libgcc',
      '-static-libstdc++',
      '-Wl,--allow-multiple-definition',
    ]
  endif
elif host_system == 'darwin'
  platform_deps += [
    dependency('appleframeworks', modules: [
      'CoreVideo',
      'IOKit',
      'Cocoa',
      'GLUT',
      'OpenGL',
      'CoreAudio',
      'AudioToolbox',
      'AudioUnit',
    ])
  ]
elif host_system == 'linux'
  platform_deps += [
    cpp.find_library('GL'),
    cpp.find_library('m'),
    cpp.find_library('pthread'),
    cpp.find_library('dl'),
    cpp.find_library('rt'),
    dependency('x11'),
    cpp.find_library('asound'),
  ]
  
  jack_dep = cpp.find_library('jack', required: false)
  if jack_dep.found()
    platform_deps += jack_dep
    add_project_arguments('-DHAVE_JACK', language: 'cpp')
  endif
  
  pulse_dep = cpp.find_library('pulse', required: false)
  if pulse_dep.found()
    platform_deps += pulse_dep
    add_project_arguments('-DHAVE_PULSE', language: 'cpp')
  endif
endif

all_deps = [
  raylib_dep,
  # sdl2_dep,
  sndfile_dep,
  samplerate_dep,
  portaudio_dep,
  rapidjson_dep,
  tomlplusplus_dep,
  spdlog_dep,
  digestpp_dep,
] + platform_deps

# ────────────────────────────────────────────────
#  source files
# ────────────────────────────────────────────────

src_files = []
inc_dirs = []

if host_machine.system() == 'windows'
  src_files += run_command(
    'powershell', '-NoProfile', '-Command',
    'Get-ChildItem src -Recurse -Filter *.cpp | ForEach-Object { $_.FullName }',
    check: true,
    capture: true
  ).stdout().strip().split('\n')
else
  src_files += run_command(
    'find', 'src', '-name', '*.cpp', '-type', 'f',
    check: true,
    capture: true
  ).stdout().strip().split('\n')
endif

inc_dirs += include_directories(
  'src/libs/audio'
)

# ────────────────────────────────────────────────
#  Executable
# ────────────────────────────────────────────────

executable('YataiDON',
  src_files,
  include_directories: inc_dirs,
  dependencies: all_deps,
  cpp_args: common_args + debug_args + sanitizer_args,
  link_args: platform_link_args + sanitizer_link_args,
  install: true,
  install_dir: 'bin'
)

summary({
  'Project': meson.project_name(),
  'Version': meson.project_version(),
  'Build type': get_option('buildtype'),
  'C++ Standard': get_option('cpp_std'),
  'Platform': host_system,
  'PortAudio Library': portaudio_lib_path,
  'Sanitizers': sanitizers ? 'AddressSanitizer, LeakSanitizer, UndefinedBehaviorSanitizer' : 'Disabled',
}, section: 'Configuration')

summary({
  'SDL2': sdl2_dep.found() ? 'found' : 'NOT FOUND',
  'raylib': raylib_dep.found() ? 'found' : 'NOT FOUND',
  'rapidjson': 'found (header-only)',
  'tomlplusplus': 'found',
  'spdlog': spdlog_dep.found() ? 'found' : 'NOT FOUND',
  'libsndfile': sndfile_dep.found() ? 'found' : 'NOT FOUND',
  'libsamplerate': samplerate_dep.found() ? 'found' : 'NOT FOUND',
  'digestpp': 'found (header-only)',
  'portaudio': 'found (prebuilt)',
}, section: 'Dependencies')
