harness_include = ['-I' + meson.current_source_dir() / 'harness']

# The shared parts of the harness, which every backend links.
harness_sources = []
foreach line : import('fs').read('harness/sources.txt').split('\n')
  if line != '' and not line.startswith('#')
    harness_sources += 'harness' / line
  endif
endforeach
harness_lib = static_library(
  'harness',
  harness_sources,
  c_args: core_include,
  implicit_include_directories: false,
  link_with: [
    core_archive_lib, core_image_lib, core_ini_lib, core_hash_lib, core_vfs_lib,
    core_io_lib, core_args_lib, core_base_lib,
  ],
  dependencies: [zlib, cc_libm],
)
# The native backend stands in for shell/game, so a native backend program
# links it instead of shell/game.
harness_native_lib = static_library(
  'harness_native',
  'harness/native.c',
  shell_null_files,
  shell_null_devices,
  c_args: core_include + shell_include,
  implicit_include_directories: false,
  link_with: shell_fonts_lib,
  dependencies: [sdl3, cc_libm],
)

th06_harness_lib = static_library(
  'th06_harness',
  'th06/harness.c',
  'th06/native_state.c',
  c_args: [layer_include['th06']] + stg_include + harness_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
)

bot_include = ['-I' + meson.current_source_dir() / 'bot']

# The recorder of new replay files: the bot, its program and the shell/game
# functions that it stands in for.
bot_lib = static_library(
  'bot',
  'bot/bot.c',
  'bot/main.c',
  'bot/shell.c',
  shell_null_files,
  shell_null_devices,
  c_args: core_include + stg_include + shell_include,
  implicit_include_directories: false,
  dependencies: [sdl3, cc_libm],
)
