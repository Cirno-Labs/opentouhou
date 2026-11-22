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
  'data.c',
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
shell_null_files = files('data.c', 'null/files.c')
shell_null_devices = files('null/devices.c')

# The built-in files of the games below builtin. Each game lists the ones
# that it carries, and the build packs them into a zip that is linked into
# that game's programs (see the games in meson.build).
shell_builtin_dir = meson.current_source_dir() / 'builtin'
shell_pack_zip = files('../../tools/meson/pack_zip')
shell_embed_binary = files('../../tools/meson/embed_binary')

# The fonts of each game: the game executables and the native backends
# link them, with the game's built-in files.
shell_fonts_lib = static_library(
  'shell_fonts',
  'game/fonts.c',
  c_args: shell_include,
  implicit_include_directories: false,
)

# shell/game runs the game, so only a game executable links it. The
# harness's backends, the recorders and the tests replace its game-facing
# functions with stand-ins.
shell_game_dep = declare_dependency(
  compile_args: [layer_include['shell']],
  link_with: [shell_game_lib, shell_fonts_lib, shell_lib],
  dependencies: core_dep,
)
