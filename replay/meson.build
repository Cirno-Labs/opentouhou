replay_include = ['-I' + meson.current_source_dir() / 'core']

replay_case_lib = static_library(
  'replay_case',
  'core/case.c',
  c_args: core_include + replay_include,
  implicit_include_directories: false,
  dependencies: [zlib],
)

runner_backend = host_machine.system() == 'windows' ? 'th06/runner/backend_windows.c' : 'th06/runner/backend_posix.c'
th06_runner_lib = static_library(
  'th06_runner',
  'th06/runner/app_null.c',
  'th06/runner/compare.c',
  'th06/runner/run.c',
  'th06/runner/state.c',
  runner_backend,
  c_args: [layer_include['th06']] + core_include + stg_include,
  implicit_include_directories: false,
  dependencies: [sdl3, zlib, cc_libm],
  link_with: [
    th06_lib, replay_case_lib, stg_pbg3_lib, stg_rng_lib, core_vfs_lib,
    core_io_lib, core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
  ],
)

