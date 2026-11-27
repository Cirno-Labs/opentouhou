test(
  'stg/note',
  executable(
    'test_note',
    'test_note.c',
    '../gfx_recorder.c',
    implicit_include_directories: false,
    link_with: [
      stg_note_lib, stg_translation_lib, core_patch_lib, core_json_lib,
      core_io_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': tests_tmp / 'stg_note',
    'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
  },
  suite: 'unit',
)

test(
  'stg/end_diff',
  executable(
    'test_end_diff',
    'test_end_diff.c',
    implicit_include_directories: false,
    link_with: [stg_end_diff_lib, core_json_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  env: {'TEST_DIR': tests_tmp / 'stg_end_diff'},
  suite: 'unit',
)

test(
  'stg/msg_diff',
  executable(
    'test_msg_diff',
    'test_msg_diff.c',
    implicit_include_directories: false,
    link_with: [stg_msg_diff_lib, core_json_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  env: {'TEST_DIR': tests_tmp / 'stg_msg_diff'},
  suite: 'unit',
)

test(
  'stg/translation',
  executable(
    'test_translation',
    'test_translation.c',
    implicit_include_directories: false,
    link_with: [stg_translation_lib, core_patch_lib, core_json_lib, core_io_lib],
    dependencies: [sdl3],
  ),
  env: {'TEST_DIR': tests_tmp / 'stg_translation'},
  suite: 'unit',
)

test(
  'stg/chain',
  executable(
    'test_chain',
    'test_chain.c',
    implicit_include_directories: false,
    link_with: [stg_chain_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'stg/angle',
  executable(
    'test_stg_angle',
    'test_stg_angle.c',
    implicit_include_directories: false,
    link_with: [stg_angle_lib, core_math_lib],
  ),
  suite: 'unit',
)

test(
  'stg/replay_files',
  executable(
    'test_stg_replay_files',
    'test_stg_replay_files.c',
    implicit_include_directories: false,
    link_with: [stg_replay_files_lib],
    dependencies: [test_deps],
  ),
  suite: 'unit',
)

test(
  'stg/rng',
  executable(
    'test_rng',
    'test_rng.c',
    implicit_include_directories: false,
    link_with: [stg_rng_lib],
  ),
  suite: 'unit',
)

test(
  'stg/wave',
  executable(
    'test_stg_wave',
    'test_stg_wave.c',
    implicit_include_directories: false,
    link_with: [stg_wave_lib],
    dependencies: [test_deps],
  ),
  suite: 'unit',
)

test(
  'stg/timer',
  executable(
    'test_timer',
    'test_timer.c',
    implicit_include_directories: false,
    link_with: [stg_timer_lib],
  ),
  suite: 'unit',
)
test(
  'stg/lzss',
  executable(
    'test_lzss',
    'test_lzss.c',
    '../pbg3_builder.c',
    implicit_include_directories: false,
    link_with: [stg_lzss_lib, core_hash_lib, core_io_lib, core_base_lib],
    dependencies: [test_deps],
  ),
  suite: 'unit',
)

test(
  'stg/pbg3',
  executable(
    'test_pbg3',
    'test_pbg3.c',
    '../pbg3_builder.c',
    implicit_include_directories: false,
    link_with: [
      stg_pbg3_lib, core_archive_lib, core_vfs_lib, core_io_lib, core_base_lib,
    ],
    dependencies: [test_deps],
  ),
  suite: 'unit',
)

test(
  'stg/pbg4',
  executable(
    'test_pbg4',
    'test_pbg4.c',
    implicit_include_directories: false,
    link_with: [
      stg_pbg4_lib, core_archive_lib, core_vfs_lib, core_io_lib, core_base_lib,
    ],
    dependencies: [test_deps],
  ),
  suite: 'unit',
)
