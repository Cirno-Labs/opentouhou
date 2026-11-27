test(
  'th01/entry',
  executable(
    'test_th01_entry',
    'test_th01_entry.c',
    '../../src/th01/entry.c',
    c_args: ['-DSDL_MAIN_HANDLED'],
    implicit_include_directories: false,
    dependencies: [sdl3],
    link_with: test_lib,
  ),
  suite: 'unit',
)
th01_text_fixture = files('th01_text_fixture.c')
th01_boss_fixtures = files(
  'th01_assets_b05.c',
  'th01_assets_b10j.c',
  'th01_assets_b10m.c',
  'th01_assets_b15j.c',
  'th01_assets_b15m.c',
  'th01_assets_b20j.c',
  'th01_assets_b20m.c',
)
test(
  'th01/main',
  executable(
    'test_th01_main',
    'test_th01_main.c',
    'th01_assets.c',
    th01_text_fixture,
    th01_boss_fixtures,
    '../audio_backend_fake.c',
    implicit_include_directories: false,
    link_with: [th01_main_lib, th01_lib, core_audio_lib, core_text_lib,
      core_image_lib, core_hash_lib, core_config_lib, core_ini_lib,
      core_io_lib, core_base_lib],
    dependencies: [pc98_dep, sdl3_ttf, sdl3, avcodec, avutil, avformat,
      swresample, cc_libm],
  ),
  suite: 'unit',
)
test(
  'th01/text',
  executable(
    'test_th01_text',
    'test_th01_text.c',
    th01_text_fixture,
    implicit_include_directories: false,
    link_with: [
      th01_lib, core_text_lib, core_image_lib, core_io_lib, core_base_lib,
      test_fonts_lib,
    ],
    dependencies: [pc98_dep, sdl3_ttf, sdl3, cc_libm],
  ),
  env: {'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts'},
  suite: 'unit',
)
test(
  'th01/opening',
  executable(
    'test_th01_opening',
    'test_th01_opening.c',
    th01_text_fixture,
    '../input_backend_fake.c',
    implicit_include_directories: false,
    link_with: [th01_lib, core_text_lib, core_image_lib, core_input_lib, core_config_lib, core_ini_lib, core_vfs_lib, core_io_lib, core_base_lib],
    dependencies: [pc98_dep, sdl3_ttf, sdl3, cc_libm],
  ),
  suite: 'unit',
)
test(
  'th01/input',
  executable(
    'test_th01_input',
    'test_th01_input.c',
    '../input_backend_fake.c',
    implicit_include_directories: false,
    link_with: [th01_lib, core_input_lib, core_config_lib, core_ini_lib, core_io_lib, core_base_lib],
    dependencies: [pc98_dep, sdl3],
  ),
  suite: 'unit',
)
test(
  'th01/audio',
  executable(
    'test_th01_audio',
    'test_th01_audio.c',
    '../audio_backend_fake.c',
    implicit_include_directories: false,
    link_with: [th01_lib, core_audio_lib, core_io_lib, core_base_lib],
    dependencies: [pc98_dep, sdl3, avcodec, avutil, avformat, swresample, cc_libm],
  ),
  suite: 'unit',
)
th01_text_fixture_parts = [
  'boss_b05', 'boss_b10j', 'boss_b10m', 'boss_b15j', 'boss_b15m', 'boss_b20j',
  'boss_b20m', 'boss_hit', 'hud', 'items', 'route_select', 'stage', 'timer',
  'bonus',
]
foreach part : ['bos', 'boss_b05', 'boss_b10m', 'boss_b10j', 'boss_b15j', 'boss_b15m', 'boss_b20j', 'boss_b20m', 'boss_entity', 'boss_render', 'boss_hit', 'boss_defeat', 'route_select', 'sprite', 'shape', 'particles', 'wave', 'interleave', 'transition', 'scale2x', 'hud', 'hud_hp', 'extend', 'timer', 'items', 'bonus', 'shots', 'pellet_assets', 'shootout_lasers', 'pellets', 'missiles', 'line_bullets', 'player', 'bomb', 'miss', 'vsync_irq', 'wait', 'finite', 'orb', 'settings', 'score_data', 'score', 'ending', 'session', 'invincibility', 'gameplay', 'video', 'palette', 'tram', 'ptn', 'grc', 'grf', 'grz', 'stage_data', 'scene', 'stage_objects', 'stage', 'stage_obstacles']
  part_sources = []
  if part == 'gameplay'
    part_sources = ['../audio_backend_fake.c', 'th01_assets.c', th01_text_fixture] + th01_boss_fixtures
  elif part == 'grz'
    part_sources = ['th01_grz_fixture.c']
  elif part in th01_text_fixture_parts
    part_sources = th01_text_fixture
  endif
  test(
    'th01/' + part,
    executable(
      'test_th01_' + part,
      'test_th01_' + part + '.c',
      part_sources,
      implicit_include_directories: false,
      link_with: [th01_lib, core_hash_lib, core_config_lib, core_ini_lib, core_vfs_lib, core_image_lib, core_io_lib, core_base_lib] + (part in ['gameplay', 'score', 'ending'] ? [core_text_lib] : []) + (part == 'gameplay' ? [core_audio_lib] : []),
      dependencies: [pc98_dep, sdl3] + (part == 'gameplay' ? [avcodec, avutil, avformat, swresample, cc_libm] : []),
    ),
    suite: 'unit',
    env: {
      'TEST_DIR': tests_tmp / 'th01' / part,
      'TEST_SPRITE_DIR': meson.project_source_root() / 'src' / 'th01' / 'sprites',
    },
  )
endforeach
test(
  'replay/th01',
  executable(
    'test_th01_harness',
    'test_th01_harness.c',
    implicit_include_directories: false,
    link_with: [th01_harness_lib, th01_lib, core_hash_lib, core_config_lib, core_ini_lib, core_vfs_lib, core_image_lib, core_io_lib, core_base_lib],
    dependencies: [pc98_dep, sdl3],
  ),
  suite: 'unit',
  env: {'TEST_DIR': tests_tmp / 'th01' / 'harness'},
)
