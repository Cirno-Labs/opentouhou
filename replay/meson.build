replay_include = ['-I' + meson.current_source_dir() / 'core']

replay_script_lib = static_library(
  'replay_script',
  'core/script.c',
  'core/session.c',
  c_args: core_include + replay_include,
  implicit_include_directories: false,
  link_with: [core_io_lib, core_base_lib],
)

replay_case_lib = static_library(
  'replay_case',
  'core/case.c',
  c_args: core_include + replay_include,
  implicit_include_directories: false,
  dependencies: [zlib],
)

runner_debug = host_machine.system() == 'windows' ? 'core/runner/debug_windows.c' : 'core/runner/debug_posix.c'
replay_runner_lib = static_library(
  'replay_runner',
  'core/runner/shell_null.c',
  'core/runner/compare.c',
  'core/runner/run.c',
  'core/runner/main.c',
  runner_debug,
  c_args: core_include + replay_include + stg_include + shell_include,
  implicit_include_directories: false,
  dependencies: [sdl3, zlib, cc_libm],
  link_with: [replay_case_lib, core_base_lib, core_io_lib],
)

replay_recorder_lib = static_library(
  'replay_recorder',
  'core/recorder/main.c',
  'core/recorder/bot.c',
  'core/runner/shell_null.c',
  c_args: core_include + replay_include + stg_include + shell_include,
  implicit_include_directories: false,
  dependencies: [sdl3, zlib, cc_libm],
  link_with: [core_base_lib, stg_rng_lib],
)

th06_runner_lib = static_library(
  'th06_runner',
  'th06/runner/game.c',
  'th06/runner/state.c',
  c_args: [layer_include['th06']] + core_include + replay_include + stg_include + shell_include,
  implicit_include_directories: false,
  dependencies: [sdl3, zlib, cc_libm],
  link_with: [
    th06_lib, replay_case_lib, stg_pbg3_lib, stg_rng_lib, stg_translation_lib,
    stg_note_lib, core_archive_lib, core_vfs_lib, core_io_lib, core_math_lib,
    core_base_lib, core_gfx_lib, core_image_lib,
  ],
)

th07_runner_lib = static_library(
  'th07_runner',
  'th07/runner/game.c',
  'th07/runner/replay.c',
  'th07/runner/state.c',
  '../src/th07/play_time.c',
  c_args: [layer_include['th07']] + core_include + replay_include + stg_include + shell_include,
  implicit_include_directories: false,
  dependencies: [sdl3, zlib, cc_libm],
  link_with: [
    th07_lib, replay_case_lib, stg_pbg3_lib, stg_rng_lib, core_archive_lib,
    core_vfs_lib, core_io_lib, core_math_lib, core_base_lib, core_gfx_lib,
    core_image_lib,
  ],
)

foreach game : ['th06', 'th07']
  session_sources = game == 'th07' ? ['../src/th07/play_time.c'] : []
  executable(
    game + 'session',
    game / 'session.c',
    'core/session_native.c',
    'core/session_compare.c',
    'core/runner/shell_null.c',
    session_sources,
    c_args: [layer_include[game]] + core_include + replay_include + stg_include + shell_include,
    implicit_include_directories: false,
    dependencies: [core_dep, stg_dep, sdl3, sdl3_ttf, zlib],
    link_with: [get_variable(game + '_main_lib'), get_variable(game + '_lib'), replay_script_lib, core_args_lib, get_variable(game + '_runner_lib'), replay_runner_lib],
    win_subsystem: 'console',
  )
endforeach
