# Each runs test_<part>.c, part being its name with underscores for
# slashes unless it gives its own, with the extra sources that it lists.
# The variables in env point TEST_DIR and XDG_DATA_HOME to its directory
# under tmp, named after dir or part, and TEST_FONT_DIR to the built-in
# fonts.
th07_data = ['th07_anm_builder.c', 'th07_data.c', '../gfx_recorder.c']
th07_test_libs = [
  th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib, stg_pbg4_lib, stg_lzss_lib,
  stg_end_diff_lib, core_patch_lib, core_json_lib, core_vfs_lib, core_archive_lib, core_audio_lib,
  core_config_lib, core_ini_lib, core_io_lib, core_math_lib, core_base_lib, core_gfx_lib,
  core_image_lib,
]
test_fonts = meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts'
shell_fake = files('../shell_fake.c') + shell_null_files
th07_tests = {
    'th07/buttons': {'deps': [test_deps]},
    'th07/gui': {
      'sources': th07_data + ['th07_gui_setup.c', 'th07_msg_builder.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    # The _ms tests run again against MS Gothic where $OPENTOUHOU_MSGOTHIC
    # names a msgothic.ttc.
    'th07/gui/text_image': {
      'part': 'th07_text_image',
      'sources': th07_data + ['../../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME', 'TEST_FONT_DIR'],
      'dir': 'test_th07_text_image',
    },
    'th07/gui/text_image/ms_gothic': {
      'part': 'th07_text_image_ms',
      'main': 'test_th07_text_image.c',
      'sources': th07_data + ['../../src/th07/play_time.c'],
      'c_args': ['-DTEST_MS_GOTHIC'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME', 'TEST_FONT_DIR'],
      'dir': 'test_th07_text_image_ms',
    },
    'th07/gui_msg': {
      'sources': th07_data + ['th07_gui_setup.c', 'th07_msg_builder.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th07/replay': {'sources': ['replay_lzss_literals.c'], 'deps': [test_deps]},
    'th07/score': {'sources': ['replay_lzss_literals.c'], 'deps': [test_deps]},
    'th07/result_saves': {'env': ['TEST_DIR']},
    'th07/screens/result': {
      'part': 'th07_result_screen',
      'sources': th07_data,
      'env': ['TEST_DIR'],
    },
    'th07/screens/ending': {'part': 'th07_ending', 'sources': th07_data, 'env': ['TEST_DIR']},
    'th07/screens/music': {'part': 'th07_music_screen', 'sources': th07_data, 'env': ['TEST_DIR']},
    'th07/replay/chain': {'sources': ['replay_lzss_literals.c'], 'deps': [test_deps]},
    'th07/game_manager': {'deps': [test_deps]},
    'th07/game_manager/chain': {'deps': [test_deps]},
    'th07/play_time': {'sources': ['../../src/th07/play_time.c'], 'deps': [test_deps]},
    'th07/scene': {
      'sources': th07_data + ['../../src/th07/play_time.c'],
      'env': ['TEST_DIR'],
    },
    'th07/anm': {
      'sources': th07_data + ['../../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th07/stage': {
      'sources': th07_data + ['../../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th07/anm/vm': {'sources': ['../../src/th07/play_time.c']},
    'th07/anm/draw': {
      'sources': ['../../src/th07/play_time.c', '../gfx_recorder.c'],
      'env': ['TEST_FONT_DIR'],
    },
    'th07/anm/text': {
      'sources': ['../../src/th07/play_time.c', '../gfx_recorder.c'],
      'env': ['TEST_FONT_DIR'],
    },
    'th07/anm/text/ms_gothic': {
      'part': 'th07_anm_text_ms',
      'main': 'test_th07_anm_text.c',
      'sources': ['../../src/th07/play_time.c', '../gfx_recorder.c'],
      'c_args': ['-DTEST_MS_GOTHIC'],
      'env': ['TEST_FONT_DIR'],
    },
    'th07/screen_effect': {'sources': ['../../src/th07/play_time.c', '../gfx_recorder.c']},
    'th07/screens/title': {
      'part': 'th07_title',
      'sources': th07_data + ['../../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'TEST_FONT_DIR'],
    },
    'th07/ascii': {
      'sources': th07_data + ['../../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th07/ascii_menu': {
      'sources': th07_data + ['../../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th07/sound': {
      'sources': ['../audio_backend_fake.c', 'th07_data.c', '../../src/th07/play_time.c'],
      'deps': [sdl3, avcodec, avutil, avformat, swresample, cc_libm],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th07/files': {'sources': ['th07_data.c'], 'env': ['TEST_DIR', 'XDG_DATA_HOME']},
    'th07/effect': {
      'sources': th07_data + ['../../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th07/player_shot_type': {'sources': ['th07_data.c'], 'env': ['TEST_DIR']},
    'th07/bomb': {'sources': th07_data, 'env': ['TEST_DIR', 'XDG_DATA_HOME']},
    'th07/item': {'sources': th07_data, 'env': ['TEST_DIR', 'XDG_DATA_HOME']},
    'th07/enemy': {'sources': th07_data, 'env': ['TEST_DIR', 'XDG_DATA_HOME']},
    'th07/ecl': {
      'sources': th07_data + ['th07_ecl_builder.c', 'th07_play_setup.c'],
      'env': ['TEST_DIR'],
    },
    'th07/ecl_patterns': {'sources': th07_data + ['th07_play_setup.c'], 'env': ['TEST_DIR']},
    'th07/player': {'sources': th07_data, 'env': ['TEST_DIR', 'XDG_DATA_HOME']},
    'th07/bullet': {'sources': th07_data, 'env': ['TEST_DIR']},
    'th07/player_shots': {'sources': ['../gfx_recorder.c']},
}
foreach name, t : th07_tests
  part = t.get('part', name.replace('/', '_'))
  env = {}
  foreach var : t.get('env', [])
    env += {var: var == 'TEST_FONT_DIR' ? test_fonts : tests_tmp / t.get('dir', part)}
  endforeach
  test(
    name,
    executable(
      'test_' + part,
      t.get('main', 'test_' + part + '.c'),
      t.get('sources', []),
      shell_fake,
      c_args: t.get('c_args', []),
      implicit_include_directories: false,
      link_with: th07_test_libs,
      dependencies: t.get('deps', [sdl3]),
    ),
    env: env,
    suite: 'unit',
  )
endforeach

test(
  'th07/msg_builder',
  executable(
    'test_th07_msg_builder',
    'test_th07_msg_builder.c',
    'th07_msg_builder.c',
    implicit_include_directories: false,
    link_with: core_base_lib,
  ),
  suite: 'unit',
)

