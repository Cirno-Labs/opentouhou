# Each runs test_<part>.c, part being its name with underscores for
# slashes unless it gives its own, with the extra sources that it lists.
# The variables in env point TEST_DIR and XDG_DATA_HOME to its directory
# under tmp, named after dir or part, and TEST_FONT_DIR to the built-in
# fonts.
th06_data = ['../gfx_recorder.c', 'anm_builder.c', '../pbg3_builder.c', 'th06_data.c']
th06_test_libs = [
  th06_lib, stg_translation_lib, stg_note_lib, stg_end_diff_lib, stg_rng_lib, stg_pbg3_lib,
  core_patch_lib, core_json_lib, core_vfs_lib, core_audio_lib, core_config_lib, core_ini_lib,
  core_io_lib, core_math_lib, core_base_lib, core_gfx_lib, core_image_lib, test_fonts_lib,
]
test_fonts = meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts'
shell_fake = files('../shell_fake.c') + shell_null_files
th06_tests = {
    'th06/ascii': {'sources': ['../gfx_recorder.c'], 'deps': [test_deps]},
    'th06/ascii_menu': {'sources': ['../gfx_recorder.c'], 'deps': [test_deps]},
    'th06/replay': {'deps': [test_deps]},
    'th06/replay_format': {'deps': [test_deps]},
    'th06/game_manager/chain': {'deps': [test_deps]},
    'th06/score': {'deps': [test_deps]},
    'th06/result_saves': {'env': ['TEST_DIR']},
    'th06/screens/result': {
      'part': 'th06_result_screen',
      'sources': th06_data,
      'env': ['TEST_DIR', 'TEST_FONT_DIR'],
    },
    'th06/screens/title': {'part': 'th06_title', 'sources': th06_data, 'env': ['TEST_DIR']},
    'th06/screens/ending': {'part': 'th06_ending', 'sources': th06_data, 'env': ['TEST_DIR']},
    'th06/screens/music': {
      'part': 'th06_music_screen',
      'sources': th06_data,
      'env': ['TEST_DIR', 'TEST_FONT_DIR'],
    },
    'th06/anm': {'sources': th06_data, 'env': ['TEST_DIR', 'XDG_DATA_HOME', 'TEST_FONT_DIR']},
    'th06/buttons': {'deps': [test_deps]},
    'th06/game_manager': {'deps': [test_deps]},
    'th06/bomb': {
      'sources': th06_data + ['th06_scene_stubs.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th06/bullet': {
      'sources': th06_data + ['th06_scene_stubs.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th06/item': {
      'sources': th06_data + ['th06_scene_stubs.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th06/player': {
      'sources': th06_data + ['th06_scene_stubs.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th06/scene': {'sources': th06_data, 'env': ['TEST_DIR']},
    'th06/screen_effect': {'sources': ['../gfx_recorder.c'], 'deps': [test_deps]},
    'th06/sound': {
      'sources': ['../audio_backend_fake.c', '../pbg3_builder.c', 'th06_data.c'],
      'deps': [sdl3, avcodec, avutil, avformat, swresample, cc_libm],
      'env': ['TEST_DIR'],
    },
    'th06/stage': {'sources': th06_data, 'env': ['TEST_DIR', 'XDG_DATA_HOME']},
    'th06/ecl': {
      'sources': ['anm_builder.c', 'ecl_builder.c', '../pbg3_builder.c', 'th06_data.c', 'th06_scene_stubs.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th06/enemy': {
      'sources': th06_data + ['ecl_builder.c', 'th06_scene_stubs.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th06/effect': {'sources': th06_data, 'env': ['TEST_DIR', 'XDG_DATA_HOME']},
    'th06/gui': {
      'sources': th06_data,
      'env': ['TEST_DIR', 'XDG_DATA_HOME', 'TEST_FONT_DIR'],
    },
    'th06/files': {
      'sources': ['../pbg3_builder.c', 'th06_data.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
}
foreach name, t : th06_tests
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
      link_with: th06_test_libs,
      dependencies: t.get('deps', [sdl3]),
    ),
    env: env,
    suite: 'unit',
  )
endforeach

