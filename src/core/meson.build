core_include = [layer_include['core']]

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
  link_with: core_lib,
  link_whole: core_main_lib,
  dependencies: [sdl3.partial_dependency(links: true)],
)
