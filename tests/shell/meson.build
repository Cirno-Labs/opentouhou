node = find_program('node', required: false)
if node.found()
  test(
    'shell/web/data',
    node,
    args: ['--test', files('../web/test_data.cjs', '../web/test_audio.cjs')],
    suite: ['unit', 'tools'],
  )
endif

test(
  'shell/game/request_queue',
  executable(
    'test_request_queue',
    'test_request_queue.c',
    '../input_backend_fake.c',
    '../../src/shell/game/request_queue.c',
    c_args: shell_include + tests_include,
    implicit_include_directories: false,
    link_with: [
      core_input_lib, core_config_lib, core_ini_lib, core_io_lib,
      core_base_lib,
    ],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)

# The tests include shell's private headers, so they compile against the
# sources directly instead of the library.
test(
  'shell/game/clock',
  executable(
    'test_clock',
    'test_clock.c',
    '../../src/shell/game/clock.c',
    c_args: shell_include + tests_include,
    implicit_include_directories: false,
  ),
  suite: 'unit',
)

test(
  'shell/folders',
  executable(
    'test_folders',
    'test_folders.c',
    c_args: shell_include + tests_include,
    implicit_include_directories: false,
    link_with: [shell_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': tests_tmp / 'folders',
    'XDG_DATA_HOME': tests_tmp / 'folders',
  },
  suite: 'unit',
)

test(
  'shell/data',
  executable(
    'test_shell_data',
    'test_shell_data.c',
    '../../src/shell/data.c',
    c_args: shell_include + tests_include,
    implicit_include_directories: false,
    link_with: [
      core_vfs_lib, core_archive_lib, core_hash_lib, core_io_lib,
      core_base_lib,
    ],
    dependencies: [zlib, sdl3],
  ),
  env: {'TEST_DIR': tests_tmp / 'shell_data'},
  suite: 'unit',
)

test(
  'shell/fatal',
  executable(
    'test_fatal',
    'test_fatal.c',
    '../../src/shell/fatal.c',
    '../../src/shell/log_file.c',
    c_args: shell_include + tests_include,
    implicit_include_directories: false,
    link_with: [core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)

test(
  'shell/log_file',
  executable(
    'test_log_file',
    'test_log_file.c',
    '../../src/shell/log_file.c',
    c_args: shell_include + tests_include,
    implicit_include_directories: false,
    link_with: [core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  env: {'TEST_DIR': tests_tmp / 'log_file'},
  suite: 'unit',
)

# Runs the shell's start up to the data check, which fails before SDL
# opens a window.
test(
  'shell/game/host',
  executable(
    'test_shell_host',
    'test_shell_host.c',
    c_args: shell_include + tests_include,
    implicit_include_directories: false,
    link_with: builtin_libs['th06'],
    dependencies: [shell_game_dep, sdl3],
  ),
  env: {
    'TEST_DIR': tests_tmp / 'shell_host',
    'SDL_VIDEODRIVER': 'dummy',
  },
  suite: 'unit',
)

test(
  'shell/game/tick_rate',
  executable(
    'test_tick_rate',
    'test_tick_rate.c',
    '../../src/shell/game/tick_rate.c',
    c_args: shell_include + tests_include,
    implicit_include_directories: false,
  ),
  suite: 'unit',
)

test(
  'shell/game/screenshot',
  executable(
    'test_screenshot',
    'test_screenshot.c',
    '../../src/shell/game/screenshot.c',
    c_args: shell_include + tests_include,
    implicit_include_directories: false,
    link_with: [core_image_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
  env: ['TEST_DIR=' + tests_tmp / 'screenshot'],
)

executable(
  'test_shell_clocks',
  'test_shell_clocks.c',
  c_args: shell_include + tests_include,
  implicit_include_directories: false,
  link_with: builtin_libs['th06'],
  dependencies: [shell_game_dep, sdl3],
)

executable(
  'test_shell_save_settings',
  'test_shell_save_settings.c',
  c_args: shell_include + tests_include,
  implicit_include_directories: false,
  link_with: builtin_libs['th06'],
  dependencies: [shell_game_dep, sdl3],
)

test(
  'shell/game/fonts',
  executable(
    'test_shell_fonts',
    'test_shell_fonts.c',
    c_args: shell_include + tests_include + [
      '-DTEST_BUILTIN_FONTS=' + (get_option('font') == 'none' ? 'false' : 'true'),
    ],
    implicit_include_directories: false,
    link_with: [
      shell_fonts_lib, builtin_libs['th06'], core_archive_lib, core_vfs_lib, core_text_lib,
      core_image_lib, core_hash_lib, core_io_lib, core_base_lib,
    ],
    dependencies: [sdl3_ttf, sdl3, zlib, text_system, cc_libm],
  ),
  suite: 'unit',
  env: {
    'TEST_DIR': tests_tmp / 'shell_fonts',
    'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
  },
)

