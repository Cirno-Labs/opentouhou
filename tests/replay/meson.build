
test(
  'replay/bot',
  executable(
    'test_bot',
    'test_bot.c',
    '../../replay/bot/bot.c',
    c_args: stg_include + bot_include + tests_include,
    implicit_include_directories: false,
    link_with: [stg_rng_lib, core_ini_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3, cc_libm],
  ),
  suite: 'unit',
)

test(
  'replay/bot/recorder',
  executable(
    'test_recorder',
    'test_recorder.c',
    c_args: core_include + stg_include + shell_include + bot_include + tests_include,
    implicit_include_directories: false,
    link_with: bot_lib,
    dependencies: [core_dep, stg_dep, sdl3, cc_libm],
  ),
  env: { 'TEST_DIR': tests_tmp / 'recorder' },
  suite: 'unit',
)

foreach part : ['script', 'case', 'state', 'compare', 'driver']
  test(
    'replay/harness/' + part,
    executable(
      'test_harness_' + part,
      'test_harness_' + part + '.c',
      c_args: core_include + harness_include + tests_include,
      implicit_include_directories: false,
      link_with: harness_lib,
      dependencies: [sdl3, zlib, avcodec, avutil, swscale, jpeg, cc_libm],
    ),
    env: { 'TEST_DIR': tests_tmp / 'harness_' + part },
    suite: 'unit',
  )
endforeach

test(
  'replay/dosbox',
  executable(
    'test_dosbox',
    'test_dosbox.c',
    c_args: core_include + harness_include + harness_dosbox_include + tests_include,
    implicit_include_directories: false,
    link_with: [harness_dosbox_lib, harness_lib],
    dependencies: [sdl3, zlib, avcodec, avutil, swscale, jpeg, cc_libm],
  ),
  env: { 'TEST_DIR': tests_tmp / 'dosbox' },
  suite: 'unit',
)
