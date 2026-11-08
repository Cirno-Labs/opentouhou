# The th06 capture tracer: a DLL injected into the original, 32-bit
# th06.exe under Wine, to record replay test cases. It is its own Meson
# project, cross-built for i686-w64-mingw32 (see
# tools/docker/win/meson_cross_i686.txt), and links core/base and
# core/io statically from ../../src.

project(
  'opentouhou-capture-th06',
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

core_base_lib = static_library(
  'core_base',
  src_root / 'core/base/arena.c',
  src_root / 'core/base/array.c',
  src_root / 'core/base/cp932.c',
  src_root / 'core/base/cp932_table.c',
  src_root / 'core/base/hash_map.c',
  src_root / 'core/base/log.c',
  src_root / 'core/base/memory.c',
  src_root / 'core/base/result.c',
  src_root / 'core/base/sha1.c',
  src_root / 'core/base/string.c',
  c_args: core_include,
  implicit_include_directories: false,
)

core_io_lib = static_library(
  'core_io',
  src_root / 'core/io/file.c',
  src_root / 'core/io/path.c',
  src_root / 'core/io/reader.c',
  src_root / 'core/io/writer.c',
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
)

shared_library(
  'th06capture',
  'dll.c',
  '../core/clock.c',
  '../core/iat.c',
  '../core/overlay.c',
  '../core/patch.c',
  '../core/case.c',
  name_prefix: '',
  c_args: core_include,
  implicit_include_directories: false,
  link_with: [core_base_lib, core_io_lib],
  dependencies: [sdl3, zlib],
)

executable(
  'th06capture',
  'launcher.c',
  '../core/launch.c',
  '../core/workdir.c',
  c_args: core_include,
  implicit_include_directories: false,
  link_with: [core_base_lib, core_io_lib],
  dependencies: [sdl3, zlib],
)

executable(
  'overlay_test',
  '../core/iat.c',
  '../core/overlay.c',
  '../core/overlay_test.c',
  c_args: core_include,
  implicit_include_directories: false,
  link_with: [core_base_lib, core_io_lib],
  dependencies: [sdl3, zlib],
)
