core_include = [layer_include['core']]

# core/base needs nothing but the C library, so it is built without SDL and
# the tests link it alone.
core_base_lib = static_library(
  'core_base',
  'base/arena.c',
  'base/array.c',
  'base/cp932.c',
  'base/cp932_table.c',
  'base/hash_map.c',
  'base/log.c',
  'base/memory.c',
  'base/result.c',
  'base/string.c',
  c_args: core_include,
  implicit_include_directories: false,
)

core_lib = static_library(
  'core',
  'app/app.c',
  'app/clock.c',
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
)

core_main_lib = static_library(
  'core_main',
  'app/main.c',
  implicit_include_directories: false,
  dependencies: [sdl3],
)

# SDL is a dependency of the library, not of its users: layers above core
# link it, but never see its headers.
core_dep = declare_dependency(
  compile_args: core_include,
  link_with: [core_lib, core_base_lib],
  link_whole: core_main_lib,
  dependencies: [sdl3.partial_dependency(links: true)],
)
