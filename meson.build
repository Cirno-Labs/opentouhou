project(
  'opentouhou',
  'c',
  version: '0',
  license: 'GPL-3.0-only',
  meson_version: '>=1.4.0',
  default_options: [
    'c_std=c23',
    'warning_level=3',
    'b_ndebug=if-release',
  ],
)

cc = meson.get_compiler('c')
python3 = import('python').find_installation()

# The game logic must reproduce the original games' floating-point
# results, so the compiler may not fuse or reorder float operations, and
# x86 uses SSE2 instead of the x87 unit.
add_project_arguments('-ffp-contract=off', '-fno-fast-math', language: 'c')
if host_machine.cpu_family() in ['x86', 'x86_64']
  add_project_arguments('-msse2', '-mfpmath=sse', language: 'c')
endif

# The container macros return typed pointers, so a pointer of the wrong
# type is an error, as it is by default in GCC 14.
add_project_arguments(
  cc.get_supported_arguments(
    '-Werror=incompatible-pointer-types',
    '-Wno-unused-parameter',
    '-Wshadow',
    '-Wstrict-prototypes',
  ),
  language: 'c',
)

if get_option('buildtype') == 'release'
  add_project_link_arguments('-s', language: 'c')
endif

if host_machine.system() == 'windows'
  # Links libgcc and winpthread into the executable, so it needs no DLLs
  # beyond the ones Windows has.
  add_project_link_arguments('-static', language: 'c')
  # Selects MinGW's own printf family, which follows C99 (%zu, %lld) on
  # every Windows version, instead of the one in msvcrt.dll.
  add_project_arguments('-D__USE_MINGW_ANSI_STDIO=1', language: 'c')
endif

# Each layer has its own include root in the build directory. The root
# holds one link, named after the layer, to the layer's source directory,
# so that a file includes "shell/game.h" and a layer cannot include a layer
# that is not on its include path.
include_root = meson.current_build_dir() / 'include'
layer_include = {}
foreach layer : ['core', 'shell', 'stg', 'th06', 'othpatch']
  run_command(
    python3,
    files('tools/meson/link_layer'),
    meson.current_source_dir() / 'src' / layer,
    include_root / layer / layer,
    check: true,
  )
  layer_include += {layer: '-I' + include_root / layer}
endforeach

programs = get_option('programs')
build_games = programs != 'othpatch'
build_othpatch = programs != 'games'

sdl3 = dependency('sdl3', version: '>=3.2.0', static: true)

# The case tools compress calc rows with zlib, and othpatch unpacks game
# packages with it. No game layer depends on it.
zlib = dependency('zlib', static: true)

if build_games
  sdl3_ttf = dependency('sdl3-ttf', version: '>=3.2.2', static: true)

  # FFmpeg supplies image codecs and audio decoding and conversion.
  # The Dockerfiles enable only the formats these modules support.
  avcodec = dependency('libavcodec', static: true)
  avutil = dependency('libavutil', static: true)
  swscale = dependency('libswscale', static: true)
  avformat = dependency('libavformat', static: true)
  swresample = dependency('libswresample', static: true)
endif

subdir('src/core')
subdir('src/shell')
if build_games
  subdir('src/stg')

  # Each game's directory adds the game to this dictionary. The executables
  # are declared here, so that they land in the root of the build directory.
  # An executable is named after the game id with an o before it, as
  # oth06, so that it does not clash with the original game's executable
  # in the same folder.
  games = {}
  subdir('src/th06')

  foreach name, game : games
    executable(
      'o' + name,
      game['sources'],
      c_args: game['c_args'],
      link_with: game.get('link_with', []),
      implicit_include_directories: false,
      dependencies: [shell_game_dep, stg_dep],
      win_subsystem: get_option('buildtype') == 'release' ? 'windows' : 'console',
    )
  endforeach

  subdir('replay')
  runner_entry = host_machine.system() == 'windows' ? 'replay/th06/runner/backend_main_windows.c' : 'replay/th06/runner/backend_main_posix.c'
  executable(
    'th06replay',
    'replay/th06/runner/main.c',
    runner_entry,
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    dependencies: [sdl3, zlib, cc_libm],
    link_with: [th06_runner_lib],
    link_args: host_machine.system() == 'windows' ? ['-municode'] : [],
    win_subsystem: 'console',
  )
endif

if build_othpatch
  # othpatch downloads patches. It is a program on core and shell alone, and
  # the only one that links curl.
  subdir('src/othpatch')
  executable(
    'othpatch',
    othpatch_entry,
    c_args: [layer_include['othpatch'], othpatch_version_include] + shell_include,
    implicit_include_directories: false,
    link_with: othpatch_libs,
    dependencies: othpatch_deps,
    link_args: host_machine.system() == 'windows' ? ['-municode'] : [],
    win_subsystem: 'console',
  )
endif

# A cross build builds the tests only for Windows, where they run outside
# the build image.
if not meson.is_cross_build() or host_machine.system() == 'windows'
  subdir('tests')
endif
