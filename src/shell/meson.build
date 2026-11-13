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

# shell/game runs the game, so only a game executable links it. The replay
# runner and the tests replace its game-facing functions with stand-ins.
shell_game_dep = declare_dependency(
  compile_args: [layer_include['shell']],
  link_with: [shell_game_lib, shell_lib],
  dependencies: core_dep,
)
