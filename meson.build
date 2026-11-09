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
# so that a file includes "core/app.h" and a layer cannot include a layer
# that is not on its include path.
include_root = meson.current_build_dir() / 'include'
layer_include = {}
foreach layer : ['core', 'stg', 'th06']
  run_command(
    python3,
    files('tools/meson/link_layer'),
    meson.current_source_dir() / 'src' / layer,
    include_root / layer / layer,
    check: true,
  )
  layer_include += {layer: '-I' + include_root / layer}
endforeach

sdl3 = dependency('sdl3', version: '>=3.2.0', static: true)

# core/image decodes and encodes through FFmpeg's libavcodec and converts
# pixel formats through libswscale, both built with only the codecs it
# uses (see tools/docker). No libavformat: core/image never demuxes a
# container.
avcodec = dependency('libavcodec', static: true)
avutil = dependency('libavutil', static: true)
swscale = dependency('libswscale', static: true)

# The replay test case format compresses its calc rows with zlib. Only the
# tests use it; no layer of the game depends on it.
zlib = dependency('zlib', static: true)

subdir('src/core')
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
    dependencies: [core_dep, stg_dep],
    win_subsystem: get_option('buildtype') == 'release' ? 'windows' : 'console',
  )
endforeach
if not meson.is_cross_build()
  subdir('tests')
endif
