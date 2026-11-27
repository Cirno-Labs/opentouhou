foreach part : [
    'input',
    'tram',
    'hud',
    'hud_wipe',
    'score',
    'draw',
    'shapes',
    'sprites_opaque',
    'sprites_zoom',
    'sprites_plane',
    'sprites_rect',
    'sprites_tiny',
    'sprites',
    'clip',
    'video',
    'tiny',
    'patterns',
    'performance',
    'background',
    'background_scroll',
    'background_setup',
    'background_render',
]
  test(
    'th02/' + part,
    executable(
      'test_th02_' + part,
      'test_th02_' + part + '.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
      implicit_include_directories: false,
      link_with: [th02_lib, core_base_lib],
      dependencies: [sdl3],
    ),
    suite: 'unit',
  )
endforeach
# These load the built-in sprite sheets from src/th02/sprites.
foreach part : [
  'sparks',
  'point_numbers',
  'bomb_assets',
  'bullets',
]
  test(
    'th02/' + part,
    executable(
      'test_th02_' + part,
      'test_th02_' + part + '.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
      implicit_include_directories: false,
      link_with: [
        th02_lib, core_image_lib, core_io_lib, core_vfs_lib, core_hash_lib,
        core_base_lib,
      ],
      dependencies: [pc98_dep, sdl3],
    ),
    suite: 'unit',
    env: {
      'TEST_SPRITE_DIR': meson.project_source_root() / 'src' / 'th02' / 'sprites',
    },
  )
endforeach
executable(
  'test_th02_presenter_gl',
  'test_th02_presenter_gl.c',
  c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
  implicit_include_directories: false,
  link_with: [th02_lib, core_gfx_gl_lib, core_gfx_lib, core_image_lib, core_math_lib, core_base_lib],
  dependencies: [sdl3, avcodec, avutil, swscale, cc_libm],
)
test(
  'th02/text_font',
  executable(
    'test_th02_text_font',
    'test_th02_text_font.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_text_lib, core_image_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3_ttf, sdl3, text_system, cc_libm],
  ),
  suite: 'unit',
)
test(
  'th02/title',
  executable(
    'test_th02_title',
    'test_th02_title.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)
foreach part : ['sound_effects']
  test(
    'th02/' + part,
    executable(
      'test_th02_' + part,
      'test_th02_' + part + '.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
      implicit_include_directories: false,
      link_with: [th02_lib, core_hash_lib, core_io_lib, core_base_lib],
      dependencies: [sdl3],
    ),
    suite: 'unit',
  )
endforeach
foreach part : ['music_wait']
  test(
    'th02/' + part,
    executable(
      'test_th02_' + part,
      'test_th02_' + part + '.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
      implicit_include_directories: false,
      link_with: [th02_lib, core_hash_lib, core_io_lib, core_base_lib],
      dependencies: [pc98_dep, sdl3],
    ),
    suite: 'unit',
  )
endforeach
test(
  'th02/game_over',
  executable(
    'test_th02_game_over',
    'test_th02_game_over.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_base_lib],
    dependencies: [pc98_dep, sdl3],
  ),
  suite: 'unit',
)
test(
  'th02/high_score_entry',
  executable(
    'test_th02_high_score_entry',
    'test_th02_high_score_entry.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)
test(
  'th02/high_score_entry_logic',
  executable(
    'test_th02_high_score_entry_logic',
    'test_th02_high_score_entry_logic.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)
test(
  'th02/high_score_view',
  executable(
    'test_th02_high_score_view',
    'test_th02_high_score_view.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)
test(
  'th02/high_score_view_render',
  executable(
    'test_th02_high_score_view_render',
    'test_th02_high_score_view_render.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)
foreach part : ['pause_menu', 'pause_menu_render']
  test(
    'th02/' + part,
    executable(
      'test_th02_' + part,
      'test_th02_' + part + '.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
      implicit_include_directories: false,
      link_with: [th02_lib, core_base_lib],
      dependencies: [sdl3],
    ),
    suite: 'unit',
  )
endforeach
foreach part : ['menu', 'options']
  test(
    'th02/' + part,
    executable(
      'test_th02_' + part,
      'test_th02_' + part + '.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
      implicit_include_directories: false,
      link_with: [th02_lib, core_base_lib],
      dependencies: [sdl3],
    ),
    suite: 'unit',
  )
endforeach
test(
  'th02/menu_render',
  executable(
    'test_th02_menu_render',
    'test_th02_menu_render.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)
test(
  'th02/options_render',
  executable(
    'test_th02_options_render',
    'test_th02_options_render.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)
foreach part : ['music_polygons']
  test(
    'th02/' + part,
    executable(
      'test_th02_' + part,
      'test_th02_' + part + '.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
      implicit_include_directories: false,
      link_with: [th02_lib, core_base_lib],
      dependencies: [pc98_dep, sdl3],
    ),
    suite: 'unit',
  )
endforeach
test(
  'th02/music_room',
  executable(
    'test_th02_music_room',
    'test_th02_music_room.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_base_lib],
    dependencies: [pc98_dep, sdl3],
  ),
  suite: 'unit',
)
test(
  'th02/music_room_render',
  executable(
    'test_th02_music_room_render',
    'test_th02_music_room_render.c',
    'th02_music_room_render.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_io_lib, core_base_lib],
    dependencies: [pc98_dep, sdl3],
  ),
  suite: 'unit',
)
test(
  'th02/music_data',
  executable(
    'test_th02_music_data',
    'test_th02_music_data.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_vfs_lib, core_io_lib, core_base_lib],
    dependencies: [pc98_dep, sdl3],
  ),
  suite: 'unit',
  env: {'TEST_DIR': tests_tmp / 'th02' / 'music_data'},
)
foreach part : ['wait']
  test(
    'th02/' + part,
    executable(
      'test_th02_' + part,
      'test_th02_' + part + '.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
      implicit_include_directories: false,
      link_with: [th02_lib, core_base_lib],
      dependencies: [pc98_dep, sdl3],
    ),
    suite: 'unit',
  )
endforeach
foreach part : ['tram_text', 'gaiji', 'palette', 'map', 'mpn']
  test(
    'th02/' + part,
    executable(
      'test_th02_' + part,
      'test_th02_' + part + '.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
      implicit_include_directories: false,
      link_with: [th02_lib, core_io_lib, core_base_lib],
      dependencies: [sdl3],
    ),
    suite: 'unit',
  )
endforeach
foreach part : [
  'items',
  'miss',
  'bomb',
  'bomb_render',
  'shots',
  'shot_fire',
  'shot_hit',
  'shot_render',
  'player',
  'player_render',
  'bullet_spawn',
  'pillars',
  'stage_effects',
  'enemies',
  'shot_select',
  'shot_select_render',
  'dialogue_box',
  'dialogue_render',
  'dialogue_sequence',
  'dialogue_slide',
  'boss',
  'boss_strategy',
  'meira',
  'rika',
  'stones',
]
  test(
    'th02/' + part,
    executable(
      'test_th02_' + part,
      'test_th02_' + part + '.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
      implicit_include_directories: false,
      link_with: [
        th02_lib, core_io_lib, core_vfs_lib, core_hash_lib, core_base_lib,
      ],
      dependencies: [pc98_dep, sdl3],
    ),
    suite: 'unit',
  )
endforeach
test(
  'th02/stage_data',
  executable(
    'test_th02_stage_data',
    'test_th02_stage_data.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)
foreach part : [
  'stage_assets', 'stage_scroll', 'stage_titles', 'stage_bonus',
  'stage_bonus_logic', 'stage_bonus_assets', 'stage_bonus_render',
]
  test(
    'th02/' + part,
    executable(
      'test_th02_' + part,
      'test_th02_' + part + '.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
      implicit_include_directories: false,
      link_with: [
        th02_lib, core_io_lib, core_vfs_lib, core_hash_lib, core_base_lib,
      ],
      dependencies: [pc98_dep, sdl3],
    ),
    suite: 'unit',
  )
endforeach
test(
  'th02/high_scores_format',
  executable(
    'test_th02_high_scores_format',
    'test_th02_high_scores_format.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)
test(
  'th02/background_assets',
  executable(
    'test_th02_background_assets',
    'test_th02_background_assets.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_vfs_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)
test(
  'th02/high_scores_file',
  executable(
    'test_th02_high_scores_file',
    'test_th02_high_scores_file.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_vfs_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
  env: {
    'TEST_DIR': tests_tmp / 'th02' / 'high_scores_file',
  },
)

test(
  'th02/settings',
  executable(
    'test_th02_settings',
    'test_th02_settings.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_config_lib, core_ini_lib, core_vfs_lib,
      core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
  env: {'TEST_DIR': tests_tmp / 'th02' / 'settings'},
)
test(
  'th02/demo',
  executable(
    'test_th02_demo',
    'test_th02_demo.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_vfs_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)
test(
  'th02/session',
  executable(
    'test_th02_session',
    'test_th02_session.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib],
  ),
  suite: 'unit',
)
test(
  'th02/random',
  executable(
    'test_th02_random',
    'test_th02_random.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_base_lib],
  ),
  suite: 'unit',
)
test(
  'th02/audio',
  executable(
    'test_th02_audio',
    'test_th02_audio.c',
    '../audio_backend_fake.c',
    c_args: [layer_include['th02'], layer_include['pc98']] + core_include + tests_include,
    implicit_include_directories: false,
    link_with: [th02_lib, core_audio_lib, core_hash_lib, core_io_lib, core_base_lib],
    dependencies: [pc98_dep, sdl3, avcodec, avutil, avformat, swresample, cc_libm],
  ),
  suite: 'unit',
)
test(
  'th02/vector',
  executable(
    'test_th02_vector',
    'test_th02_vector.c',
    c_args: tests_include,
    implicit_include_directories: false,
    dependencies: [pc98_dep, test_deps],
  ),
  suite: 'unit',
)
