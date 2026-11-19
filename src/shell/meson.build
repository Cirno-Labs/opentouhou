# shell sees core's headers and its own.
shell_include = [layer_include['core'], layer_include['shell']]

# The folder rules, the log file and the fatal box serve every program, so
# othpatch links this library too. It needs core/base and core/io only.
shell_lib = static_library(
  'shell',
  'fatal.c',
  'folders.c',
  'log_file.c',
  c_args: shell_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
)

if not build_games
  subdir_done()
endif

shell_game_lib = static_library(
  'shell_game',
  'game/clock.c',
  'game/host.c',
  'game/hotkeys.c',
  'game/request_queue.c',
  'game/screenshot.c',
  'game/settings.c',
  'game/tick_rate.c',
  c_args: shell_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
)

# The stand-ins for shell/game where a game runs without it (see null.h).
shell_null_files = files('null/files.c')
shell_null_devices = files('null/devices.c')

# shell/game runs the game, so only a game executable links it. The
# harness's backends, the recorders and the tests replace its game-facing
# functions with stand-ins.
shell_game_dep = declare_dependency(
  compile_args: [layer_include['shell']],
  link_with: [shell_game_lib, shell_lib],
  dependencies: core_dep,
)
