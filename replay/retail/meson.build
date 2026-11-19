# The retail backends: GAMEretail.exe, which starts a game's original,
# 32-bit program under Wine, and GAMEretail.dll, which it injects (see
# retail.h). It is its own Meson project, cross-built for
# i686-w64-mingw32 (see tools/docker/win/meson_cross_i686.txt), for the
# game that the game option names. It links the harness and the core
# libraries that src/core/sources.txt and ../harness/sources.txt list
# statically from ../../src, and GAME/meson.build lists the game's own
# sources.

project(
  'opentouhou-retail',
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
src_root = meson.current_source_dir() / '../../src'
core_include = ['-I' + src_root]

add_project_arguments('-ffp-contract=off', '-fno-fast-math', language: 'c')
add_project_arguments('-msse2', '-mfpmath=sse', language: 'c')
add_project_arguments(
  cc.get_supported_arguments(
    '-Werror=incompatible-pointer-types',
    '-Wno-unused-parameter',
    '-Wshadow',
    '-Wstrict-prototypes',
  ),
  language: 'c',
)
add_project_arguments('-D__USE_MINGW_ANSI_STDIO=1', language: 'c')
add_project_link_arguments('-static', language: 'c')
if get_option('buildtype') == 'release'
  add_project_link_arguments('-s', language: 'c')
endif

sdl3 = dependency('sdl3', version: '>=3.2.0', static: true)
zlib = dependency('zlib', static: true)
jpeg = dependency('libjpeg', static: true)
avcodec = dependency('libavcodec', static: true)
avutil = dependency('libavutil', static: true)
swscale = dependency('libswscale', static: true)

fs = import('fs')

# Reads the core libraries' sources from src/core/sources.txt into one
# list.
core_sources = [src_root / 'core/io/backend_windows.c']
foreach line : fs.read(src_root / 'core/sources.txt').split('\n')
  if line != '' and not line.startswith('#') and not line.startswith('[')
    core_sources += src_root / 'core' / line
  endif
endforeach

media = [sdl3, zlib, avcodec, avutil, swscale, jpeg]

core_lib = static_library(
  'core',
  core_sources,
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: media,
)

harness_include = core_include + ['-I' + meson.current_source_dir() / '../harness', '-I' + meson.current_source_dir()]

harness_sources = []
foreach line : fs.read('../harness/sources.txt').split('\n')
  if line != '' and not line.startswith('#')
    harness_sources += '../harness' / line
  endif
endforeach
harness_lib = static_library(
  'harness',
  harness_sources,
  c_args: harness_include,
  implicit_include_directories: false,
  link_with: core_lib,
  dependencies: [zlib],
)

# The parts of GAMEretail.dll and of GAMEretail.exe that every game shares.
retail_sources = files(
  'clock.c',
  'direct3d.c',
  'iat.c',
  'overlay.c',
  'patch.c',
  'retail.c',
)
launch_sources = files('launch.c')

game = get_option('game')
assert(game != '', 'Set the game option, such as -Dgame=th06')
subdir(game)

shared_library(
  game + 'retail',
  retail_sources + game_dll_sources,
  name_prefix: '',
  c_args: harness_include,
  implicit_include_directories: false,
  link_with: [harness_lib],
  dependencies: media + [cc.find_library('winmm')],
)

executable(
  game + 'retail',
  launch_sources + game_exe_sources,
  c_args: harness_include,
  implicit_include_directories: false,
  link_with: [harness_lib],
  dependencies: media,
)
