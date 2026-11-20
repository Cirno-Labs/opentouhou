# The tests of core/base, core/args, core/hash, core/json, core/io,
# core/archive/zip and the tools are in the suite core or tools. Every build
# runs them, whatever programs it has.

# Each part of core/base has a test named after it.
foreach part : [
  'arena',
  'array',
  'cp932',
  'hash_map',
  'log',
  'log_fatal_handler',
  'memory',
  'result',
  'string',
]
  test(
    'core/base/' + part,
    executable(
      'test_' + part,
      'test_' + part + '.c',
      c_args: core_include,
      implicit_include_directories: false,
      link_with: core_base_lib,
    ),
    suite: ['unit', 'core'],
  )
endforeach

# Each part of core/hash has a test named after it.
foreach part : [
  'crc32',
  'sha1',
  'sha256',
]
  test(
    'core/hash/' + part,
    executable(
      'test_' + part,
      'test_' + part + '.c',
      c_args: core_include,
      implicit_include_directories: false,
      link_with: [core_hash_lib, core_base_lib],
    ),
    suite: ['unit', 'core'],
  )
endforeach

test(
  'core/args',
  executable(
    'test_core_args',
    'test_core_args.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_args_lib, core_base_lib],
  ),
  suite: ['unit', 'core'],
)

test(
  'core/archive/zip',
  executable(
    'test_archive_zip',
    'test_archive_zip.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_archive_lib, core_hash_lib, core_io_lib, core_base_lib],
    dependencies: [zlib],
  ),
  suite: ['unit', 'core'],
)

# Each part of core/json has a test named after it.
foreach part : [
  'get',
  'index',
  'merge',
  'parse',
]
  test(
    'core/json/' + part,
    executable(
      'test_json_' + part,
      'test_json_' + part + '.c',
      c_args: core_include,
      implicit_include_directories: false,
      link_with: [core_json_lib, core_base_lib],
    ),
    suite: ['unit', 'core'],
  )
endforeach

# Each part of core/io has tests named after it. A test that works with
# files does so in its own directory inside the build directory, which is
# also its user directory.
foreach name : [
  'file',
  'locale',
  'path',
  'reader',
  'reader_file',
  'reader_owned_buffer',
  'system_path',
  'writer',
  'writer_file',
]
  test(
    'core/io/' + name,
    executable(
      'test_' + name,
      'test_' + name + '.c',
      c_args: core_include,
      implicit_include_directories: false,
      link_with: [core_io_lib, core_base_lib],
      dependencies: [sdl3],
    ),
    env: {
      'TEST_DIR': meson.current_build_dir() / 'tmp' / name,
      'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / name,
    },
    suite: ['unit', 'core'],
  )
endforeach

# These tests do something fatal. Each passes only when the program
# aborts with a fatal error that contains the given text.
python = find_program('python3.12', 'python3')
expect_fatal = files('expect_fatal.py')
foreach name, expected : {
  'core/base/allocator_failure': 'Out of memory allocating 8 bytes',
  'core/base/arena_full': 'Arena test is full',
  'core/base/array_out_of_range': 'Assertion failed: index >= 0 && index < array->count',
  'core/base/assert': 'Assertion failed: EXIT_SUCCESS == 1',
  'core/base/memory_overflow': 'elements of 3 bytes is too large',
  'core/io/reader_borrow_file': 'Assertion failed: reader->file == nullptr',
  'core/io/reader_buffer_null': 'Assertion failed: data != nullptr || size == 0',
  'core/io/writer_closed': 'Assertion failed: M_IsOpen(writer)',
  'core/io/writer_seek_past_end': 'Assertion failed: position <= writer->size',
  'core/io/writer_take_file': 'Assertion failed: writer->file == nullptr',
}
  part = name.split('/')[-1]
  test(
    name,
    python,
    args: [
      expect_fatal,
      expected,
      executable(
        'test_' + part,
        'test_' + part + '.c',
        c_args: core_include,
        implicit_include_directories: false,
        link_with: [core_io_lib, core_base_lib],
      ),
    ],
    suite: ['unit', 'core'],
  )
endforeach

test(
  'core/args/bad_rule',
  python,
  args: [
    expect_fatal,
    'args: rule names --slow, which tool lacks',
    executable(
      'test_core_args_bad_rule',
      'test_core_args_bad_rule.c',
      c_args: core_include,
      implicit_include_directories: false,
      link_with: [core_args_lib, core_base_lib],
    ),
  ],
  suite: ['unit', 'core'],
)

# The tests of the Python tools under tools/ run with the Python of the
# build. They need neither a built program nor the network.
foreach name : [
  'changelog', 'harness', 'header_spacing', 'othpatch_release', 'package_docs',
  'package_files', 'release_index', 'versioning',
]
  test(
    'tools/' + name,
    python,
    args: [files('tools/test_' + name + '.py')],
    suite: ['unit', 'tools'],
  )
endforeach

if build_othpatch
  # Each part of othpatch has a test named after it. The tests use
  # repository networks of file:// URLs in their own directories, so no test
  # needs the network. othpatch allocates its arenas with malloc, which
  # MALLOC_PERTURB_ would fill byte by byte, so the tests turn it off. The
  # tests that run a game or othpatch run copies of fake_program.
  fake_program = executable(
    'fake_program',
    'fake_program.c',
    implicit_include_directories: false,
  )
  foreach part : ['fetch', 'game', 'install', 'main', 'release', 'console']
    test(
      'othpatch/' + part,
      executable(
        'test_othpatch_' + part,
        'test_othpatch_' + part + '.c',
        c_args: [layer_include['othpatch']] + core_include,
        implicit_include_directories: false,
        link_with: othpatch_libs,
        dependencies: othpatch_deps,
      ),
      env: {
        'TEST_DIR': meson.current_build_dir() / 'tmp' / 'othpatch_' + part,
        'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'othpatch_' + part,
        'MALLOC_PERTURB_': '0',
        'FAKE_PROGRAM': fake_program.full_path(),
      },
      depends: fake_program,
      suite: 'unit',
    )
  endforeach
endif

# The other tests need the libraries of the games.
if not build_games
  subdir_done()
endif

# The game tests' stand-in for shell/game (see shell_fake.h).
shell_fake = files('shell_fake.c') + shell_null_files

test(
  'core/audio/device',
  executable(
    'test_audio_device',
    'test_audio_device.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_audio_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3, avcodec, avutil, avformat, swresample, cc_libm],
  ),
  env: {'SDL_AUDIO_DRIVER': 'dummy'},
  suite: 'unit',
)

foreach part : ['audio', 'audio_decoder', 'audio_resampler']
  test(
    'core/' + part.replace('_', '/'),
    executable(
      'test_' + part,
      'test_' + part + '.c',
      'audio_backend_fake.c',
      c_args: core_include,
      implicit_include_directories: false,
      link_with: [core_audio_lib, core_io_lib, core_base_lib],
      dependencies: [sdl3, avcodec, avutil, avformat, swresample, cc_libm],
    ),
    suite: 'unit',
  )
endforeach

foreach ascii_test : ['ascii', 'ascii_menu']
  test(
    'th06/' + ascii_test,
    executable(
      'test_th06_' + ascii_test,
      'test_th06_' + ascii_test + '.c',
      shell_fake,
      'gfx_recorder.c',
      c_args: [layer_include['th06']] + stg_include + shell_include,
      implicit_include_directories: false,
      link_with: [
        th06_lib, stg_translation_lib, stg_note_lib, core_base_lib, core_gfx_lib, core_image_lib, core_math_lib,
      ],
    ),
    suite: 'unit',
  )
endforeach

foreach replay_test : ['replay', 'replay_format']
  test(
    'th06/' + replay_test,
    executable(
      'test_th06_' + replay_test,
      'test_th06_' + replay_test + '.c',
      shell_fake,
      c_args: [layer_include['th06']] + stg_include + shell_include,
      implicit_include_directories: false,
      link_with: [
        th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, core_io_lib, core_base_lib, core_gfx_lib,
        core_image_lib, core_math_lib,
      ],
    ),
    suite: 'unit',
  )
endforeach

test(
  'th06/game_manager/chain',
  executable(
    'test_th06_game_manager_chain',
    'test_th06_game_manager_chain.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, core_io_lib, core_math_lib, core_base_lib,
      core_gfx_lib, core_image_lib,
    ],
  ),
  suite: 'unit',
)

test(
  'th06/score',
  executable(
    'test_th06_score',
    'test_th06_score.c',
    shell_fake,
    c_args: [layer_include['th06']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [th06_lib, stg_translation_lib, stg_note_lib, core_io_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'th06/result_saves',
  executable(
    'test_th06_result_saves',
    'test_th06_result_saves.c',
    shell_fake,
    c_args: [layer_include['th06']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, core_io_lib, core_base_lib, core_gfx_lib,
      core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_result_saves',
  },
  suite: 'unit',
)

test(
  'th06/screens/result',
  executable(
    'test_th06_result_screen',
    'test_th06_result_screen.c',
    shell_fake,
    'gfx_recorder.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_result_screen',
    'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
  },
  suite: 'unit',
)

test(
  'th06/screens/main',
  executable(
    'test_th06_main_menu',
    'test_th06_main_menu.c',
    shell_fake,
    'gfx_recorder.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_config_lib, core_ini_lib, core_math_lib, core_base_lib,
      core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_main_menu',
  },
  suite: 'unit',
)

test(
  'th06/screens/ending',
  executable(
    'test_th06_ending',
    'test_th06_ending.c',
    shell_fake,
    'gfx_recorder.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_ending',
  },
  suite: 'unit',
)

test(
  'th06/screens/music',
  executable(
    'test_th06_music_screen',
    'test_th06_music_screen.c',
    shell_fake,
    'gfx_recorder.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_music_screen',
    'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
  },
  suite: 'unit',
)

test(
  'th06/anm',
  executable(
    'test_th06_anm',
    'test_th06_anm.c',
    shell_fake,
    'gfx_recorder.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_base_lib, core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_anm',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_anm',
    'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
  },
  suite: 'unit',
)

test(
  'th06/buttons',
  executable(
    'test_th06_buttons',
    'test_th06_buttons.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, core_base_lib, core_gfx_lib, core_image_lib, core_math_lib,
    ],
  ),
  suite: 'unit',
)

test(
  'th06/game_manager',
  executable(
    'test_th06_game_manager',
    'test_th06_game_manager.c',
    shell_fake,
    c_args: [layer_include['th06']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, core_gfx_lib, core_image_lib, core_math_lib,
    ],
  ),
  suite: 'unit',
)

test(
  'th06/bomb',
  executable(
    'test_th06_bomb',
    'test_th06_bomb.c',
    'gfx_recorder.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_bomb',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_bomb',
  },
  suite: 'unit',
)

test(
  'th06/bullet',
  executable(
    'test_th06_bullet',
    'test_th06_bullet.c',
    'gfx_recorder.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_bullet',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_bullet',
  },
  suite: 'unit',
)

test(
  'th06/item',
  executable(
    'test_th06_item',
    'test_th06_item.c',
    'gfx_recorder.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_item',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_item',
  },
  suite: 'unit',
)

test(
  'th06/player',
  executable(
    'test_th06_player',
    'test_th06_player.c',
    'gfx_recorder.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_player',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_player',
  },
  suite: 'unit',
)

test(
  'th06/scene',
  executable(
    'test_th06_scene',
    'test_th06_scene.c',
    'th06_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, core_base_lib, core_gfx_lib, core_image_lib, core_math_lib,
    ],
  ),
  suite: 'unit',
)

test(
  'th06/screen_effect',
  executable(
    'test_th06_screen_effect',
    'test_th06_screen_effect.c',
    'gfx_recorder.c',
    'th06_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, core_math_lib, core_base_lib, core_gfx_lib,
      core_image_lib,
    ],
  ),
  suite: 'unit',
)

test(
  'th06/sound',
  executable(
    'test_th06_sound',
    'test_th06_sound.c',
    shell_fake,
    'audio_backend_fake.c',
    'pbg3_builder.c',
    'th06_data.c',
    c_args: [layer_include['th06']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_pbg3_lib, core_vfs_lib, core_audio_lib, core_io_lib,
      core_config_lib, core_ini_lib, core_math_lib, core_base_lib,
    ],
    dependencies: [sdl3, avcodec, avutil, avformat, swresample, cc_libm],
  ),
  env: { 'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_sound' },
  suite: 'unit',
)

test(
  'th06/stage',
  executable(
    'test_th06_stage',
    'test_th06_stage.c',
    'gfx_recorder.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_stage',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_stage',
  },
  suite: 'unit',
)

test(
  'th06/ecl',
  executable(
    'test_th06_ecl',
    'test_th06_ecl.c',
    'anm_builder.c',
    'ecl_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_end_diff_lib, stg_rng_lib, stg_pbg3_lib,
      core_vfs_lib, core_io_lib, core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_ecl',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_ecl',
  },
  suite: 'unit',
)

test(
  'th06/enemy',
  executable(
    'test_th06_enemy',
    'test_th06_enemy.c',
    'gfx_recorder.c',
    'anm_builder.c',
    'ecl_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_enemy',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_enemy',
  },
  suite: 'unit',
)

test(
  'th06/effect',
  executable(
    'test_th06_effect',
    'test_th06_effect.c',
    'gfx_recorder.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_effect',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_effect',
  },
  suite: 'unit',
)

test(
  'th06/gui',
  executable(
    'test_th06_gui',
    'test_th06_gui.c',
    'gfx_recorder.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th06']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_gui',
    'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_gui',
  },
  suite: 'unit',
)

test(
  'stg/note',
  executable(
    'test_note',
    'test_note.c',
    'gfx_recorder.c',
    c_args: core_include + stg_include,
    implicit_include_directories: false,
    link_with: [
      stg_note_lib, stg_translation_lib, core_patch_lib, core_json_lib,
      core_io_lib, core_base_lib, core_gfx_lib, core_image_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'stg_note',
    'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
  },
  suite: 'unit',
)

test(
  'stg/end_diff',
  executable(
    'test_end_diff',
    'test_end_diff.c',
    c_args: core_include + stg_include,
    implicit_include_directories: false,
    link_with: [stg_end_diff_lib, core_json_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  env: {'TEST_DIR': meson.current_build_dir() / 'tmp' / 'stg_end_diff'},
  suite: 'unit',
)

test(
  'stg/msg_diff',
  executable(
    'test_msg_diff',
    'test_msg_diff.c',
    c_args: core_include + stg_include,
    implicit_include_directories: false,
    link_with: [stg_msg_diff_lib, core_json_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  env: {'TEST_DIR': meson.current_build_dir() / 'tmp' / 'stg_msg_diff'},
  suite: 'unit',
)

test(
  'stg/translation',
  executable(
    'test_translation',
    'test_translation.c',
    c_args: core_include + stg_include,
    implicit_include_directories: false,
    link_with: [stg_translation_lib, core_patch_lib, core_json_lib, core_io_lib],
    dependencies: [sdl3],
  ),
  env: {'TEST_DIR': meson.current_build_dir() / 'tmp' / 'stg_translation'},
  suite: 'unit',
)

test(
  'th06/files',
  executable(
    'test_th06_files',
    'test_th06_files.c',
    shell_fake,
    'pbg3_builder.c',
    'th06_data.c',
    c_args: [layer_include['th06']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_translation_lib, stg_note_lib, stg_pbg3_lib, core_patch_lib, core_json_lib, core_vfs_lib,
      core_io_lib, core_base_lib, core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_files',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_files',
  },
  suite: 'unit',
)

test(
  'th07/buttons',
  executable(
    'test_th07_buttons',
    'test_th07_buttons.c',
    shell_fake,
    c_args: [layer_include['th07']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [th07_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'th07/gui',
  executable(
    'test_th07_gui',
    'test_th07_gui.c',
    'th07_gui_setup.c',
    'th07_anm_builder.c',
    'th07_data.c',
    'th07_msg_builder.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_gui',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th07_gui',
  },
  suite: 'unit',
)

# The translated text test runs against IPAGothic, and again against MS
# Gothic where $OPENTOUHOU_MSGOTHIC names a msgothic.ttc.
foreach variant : [['th07/gui/text_image', 'test_th07_text_image', []], ['th07/gui/text_image/ms_gothic', 'test_th07_text_image_ms', ['-DTEST_MS_GOTHIC']]]
  test(
    variant[0],
    executable(
      variant[1],
      'test_th07_text_image.c',
      'th07_anm_builder.c',
      'th07_data.c',
      'th07_scene_stubs.c',
      shell_fake,
      'gfx_recorder.c',
      c_args: [layer_include['th07']] + core_include + stg_include + shell_include + variant[2],
      implicit_include_directories: false,
      link_with: [
        th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
        stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
        core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
        core_gfx_lib, core_image_lib, core_math_lib,
      ],
      dependencies: [sdl3],
    ),
    env: {
      'TEST_DIR': meson.current_build_dir() / 'tmp' / variant[1],
      'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / variant[1],
      'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
    },
    suite: 'unit',
  )
endforeach

test(
  'th07/gui_msg',
  executable(
    'test_th07_gui_msg',
    'test_th07_gui_msg.c',
    'th07_gui_setup.c',
    'th07_anm_builder.c',
    'th07_data.c',
    'th07_msg_builder.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_gui_msg',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th07_gui_msg',
  },
  suite: 'unit',
)

test(
  'th07/msg_builder',
  executable(
    'test_th07_msg_builder',
    'test_th07_msg_builder.c',
    'th07_msg_builder.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: core_base_lib,
  ),
  suite: 'unit',
)

test(
  'th07/replay',
  executable(
    'test_th07_replay',
    'test_th07_replay.c',
    'replay_lzss_literals.c',
    shell_fake,
    c_args: [layer_include['th07']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [th07_lib, stg_lzss_lib, core_io_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'th07/score',
  executable(
    'test_th07_score',
    'test_th07_score.c',
    shell_fake,
    'replay_lzss_literals.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [th07_lib, stg_lzss_lib, core_vfs_lib, core_io_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'th07/result_saves',
  executable(
    'test_th07_result_saves',
    'test_th07_result_saves.c',
    shell_fake,
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_rng_lib, stg_lzss_lib, core_vfs_lib, core_io_lib, core_base_lib,
      core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_result_saves',
  },
  suite: 'unit',
)

test(
  'th07/screens/result',
  executable(
    'test_th07_result_screen',
    'test_th07_result_screen.c',
    'th07_anm_builder.c',
    'th07_data.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_config_lib,
      core_ini_lib, core_base_lib, core_gfx_lib, core_image_lib,
      core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_result_screen',
  },
  suite: 'unit',
)

test(
  'th07/screens/ending',
  executable(
    'test_th07_ending',
    'test_th07_ending.c',
    'th07_anm_builder.c',
    'th07_data.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, stg_end_diff_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_config_lib,
      core_ini_lib, core_base_lib, core_gfx_lib, core_image_lib,
      core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_ending',
  },
  suite: 'unit',
)

test(
  'th07/screens/music',
  executable(
    'test_th07_music_screen',
    'test_th07_music_screen.c',
    'th07_anm_builder.c',
    'th07_data.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_config_lib,
      core_ini_lib, core_base_lib, core_gfx_lib, core_image_lib,
      core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_music_screen',
  },
  suite: 'unit',
)

test(
  'th07/replay/chain',
  executable(
    'test_th07_replay_chain',
    'test_th07_replay_chain.c',
    'replay_lzss_literals.c',
    shell_fake,
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_rng_lib, stg_lzss_lib, core_io_lib,
      core_base_lib, core_math_lib,
    ],
  ),
  suite: 'unit',
)

test(
  'th07/game_manager',
  executable(
    'test_th07_game_manager',
    'test_th07_game_manager.c',
    shell_fake,
    c_args: [layer_include['th07']] + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [th07_lib, stg_rng_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'th07/game_manager/chain',
  executable(
    'test_th07_game_manager_chain',
    'test_th07_game_manager_chain.c',
    shell_fake,
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_rng_lib, core_base_lib,
    ],
  ),
  suite: 'unit',
)

test(
  'th07/play_time',
  executable(
    'test_th07_play_time',
    'test_th07_play_time.c',
    '../src/th07/play_time.c',
    shell_fake,
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [th07_lib, stg_rng_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'th07/scene',
  executable(
    'test_th07_scene',
    'test_th07_scene.c',
    'th07_scene_stubs.c',
    'th07_anm_builder.c',
    'th07_data.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_scene',
  },
  suite: 'unit',
)

test(
  'th07/anm',
  executable(
    'test_th07_anm',
    'test_th07_anm.c',
    'th07_anm_builder.c',
    'th07_data.c',
    'th07_scene_stubs.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib,
      core_json_lib, core_vfs_lib, core_archive_lib, core_io_lib,
      core_base_lib, core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_anm',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th07_anm',
  },
  suite: 'unit',
)

test(
  'th07/stage',
  executable(
    'test_th07_stage',
    'test_th07_stage.c',
    'gfx_recorder.c',
    'th07_anm_builder.c',
    'th07_data.c',
    'th07_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib,
      core_json_lib, core_vfs_lib, core_archive_lib, core_io_lib,
      core_base_lib, core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_stage',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th07_stage',
  },
  suite: 'unit',
)

test(
  'th07/anm/vm',
  executable(
    'test_th07_anm_vm',
    'test_th07_anm_vm.c',
    'th07_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)

# The text test runs against IPAGothic, and again against MS Gothic where
# $OPENTOUHOU_MSGOTHIC names a msgothic.ttc.
foreach anm_test : [
  ['th07/anm/draw', 'test_th07_anm_draw', 'test_th07_anm_draw.c', []],
  ['th07/anm/text', 'test_th07_anm_text', 'test_th07_anm_text.c', []],
  ['th07/anm/text/ms_gothic', 'test_th07_anm_text_ms', 'test_th07_anm_text.c', ['-DTEST_MS_GOTHIC']],
]
  test(
    anm_test[0],
    executable(
      anm_test[1],
      anm_test[2],
      'th07_scene_stubs.c',
      shell_fake,
      'gfx_recorder.c',
      c_args: [layer_include['th07']] + core_include + stg_include + shell_include + anm_test[3],
      implicit_include_directories: false,
      link_with: [
        th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
        stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
        core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
        core_gfx_lib, core_image_lib, core_math_lib,
      ],
      dependencies: [sdl3],
    ),
    suite: 'unit',
    env: {
      'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
    },
  )
endforeach

test(
  'th07/screen_effect',
  executable(
    'test_th07_screen_effect',
    'test_th07_screen_effect.c',
    'th07_scene_stubs.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_config_lib,
      core_ini_lib, core_base_lib, core_gfx_lib, core_image_lib,
      core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)

test(
  'th07/screens/main',
  executable(
    'test_th07_title',
    'test_th07_title.c',
    'th07_scene_stubs.c',
    'th07_anm_builder.c',
    'th07_data.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07'], '-DTH07_TITLE_TEST'] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_config_lib,
      core_ini_lib, core_base_lib, core_gfx_lib, core_image_lib,
      core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_title',
    'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
  },
  suite: 'unit',
)

foreach ascii_test : ['ascii', 'ascii_menu']
  test(
    'th07/' + ascii_test,
    executable(
      'test_th07_' + ascii_test,
      'test_th07_' + ascii_test + '.c',
      'th07_anm_builder.c',
      'th07_data.c',
      'th07_scene_stubs.c',
      shell_fake,
      'gfx_recorder.c',
      c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
      implicit_include_directories: false,
      link_with: [
        th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
        stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
        core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
        core_gfx_lib, core_image_lib, core_math_lib,
      ],
      dependencies: [sdl3],
    ),
    env: {
      'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_' + ascii_test,
      'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th07_' + ascii_test,
    },
    suite: 'unit',
  )
endforeach

test(
  'th07/sound',
  executable(
    'test_th07_sound',
    'test_th07_sound.c',
    'audio_backend_fake.c',
    'th07_data.c',
    'th07_scene_stubs.c',
    shell_fake,
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_pbg4_lib, stg_lzss_lib, core_patch_lib,
      core_json_lib, core_vfs_lib, core_archive_lib, core_audio_lib,
      core_io_lib, core_config_lib, core_ini_lib, core_math_lib,
      core_base_lib,
    ],
    dependencies: [sdl3, avcodec, avutil, avformat, swresample, cc_libm],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_sound',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th07_sound',
  },
  suite: 'unit',
)

test(
  'th07/files',
  executable(
    'test_th07_files',
    'test_th07_files.c',
    shell_fake,
    'th07_data.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_files',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th07_files',
  },
  suite: 'unit',
)

test(
  'stg/chain',
  executable(
    'test_chain',
    'test_chain.c',
    c_args: stg_include,
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
    c_args: stg_include,
    implicit_include_directories: false,
    link_with: [stg_angle_lib, core_math_lib],
  ),
  suite: 'unit',
)

test(
  'stg/rng',
  executable(
    'test_rng',
    'test_rng.c',
    c_args: stg_include,
    implicit_include_directories: false,
    link_with: [stg_rng_lib],
  ),
  suite: 'unit',
)

test(
  'stg/timer',
  executable(
    'test_timer',
    'test_timer.c',
    c_args: stg_include,
    implicit_include_directories: false,
    link_with: [stg_timer_lib],
  ),
  suite: 'unit',
)

test(
  'replay/bot',
  executable(
    'test_bot',
    'test_bot.c',
    '../replay/bot/bot.c',
    c_args: stg_include + bot_include,
    implicit_include_directories: false,
    link_with: [stg_rng_lib, core_base_lib],
    dependencies: [cc_libm],
  ),
  suite: 'unit',
)

foreach part : ['script', 'case', 'state', 'compare', 'driver']
  test(
    'replay/harness/' + part,
    executable(
      'test_harness_' + part,
      'test_harness_' + part + '.c',
      c_args: core_include + harness_include,
      implicit_include_directories: false,
      link_with: harness_lib,
      dependencies: [sdl3, zlib, avcodec, avutil, swscale, jpeg, cc_libm],
    ),
    env: { 'TEST_DIR': meson.current_build_dir() / 'tmp' / 'harness_' + part },
    suite: 'unit',
  )
endforeach

# The tests include shell's private headers, so they compile against the
# sources directly instead of the library.
test(
  'shell/game/clock',
  executable(
    'test_clock',
    'test_clock.c',
    '../src/shell/game/clock.c',
    c_args: shell_include,
    implicit_include_directories: false,
  ),
  suite: 'unit',
)

test(
  'shell/folders',
  executable(
    'test_folders',
    'test_folders.c',
    c_args: shell_include,
    implicit_include_directories: false,
    link_with: [shell_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'folders',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'folders',
  },
  suite: 'unit',
)

test(
  'shell/fatal',
  executable(
    'test_fatal',
    'test_fatal.c',
    '../src/shell/fatal.c',
    '../src/shell/log_file.c',
    c_args: shell_include,
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
    '../src/shell/log_file.c',
    c_args: shell_include,
    implicit_include_directories: false,
    link_with: [core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  env: {'TEST_DIR': meson.current_build_dir() / 'tmp' / 'log_file'},
  suite: 'unit',
)

# Runs the shell's start up to the data check, which fails before SDL
# opens a window.
test(
  'shell/game/host',
  executable(
    'test_shell_host',
    'test_shell_host.c',
    c_args: shell_include,
    implicit_include_directories: false,
    link_with: builtin_libs['th06'],
    dependencies: [shell_game_dep, sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'shell_host',
    'SDL_VIDEODRIVER': 'dummy',
  },
  suite: 'unit',
)

test(
  'shell/game/tick_rate',
  executable(
    'test_tick_rate',
    'test_tick_rate.c',
    '../src/shell/game/tick_rate.c',
    c_args: shell_include,
    implicit_include_directories: false,
  ),
  suite: 'unit',
)

test(
  'shell/game/screenshot',
  executable(
    'test_screenshot',
    'test_screenshot.c',
    '../src/shell/game/screenshot.c',
    c_args: shell_include,
    implicit_include_directories: false,
    link_with: [core_image_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
  env: ['TEST_DIR=' + meson.current_build_dir() / 'tmp/screenshot'],
)

# Each part of core/math has a test named after it, linking the sources
# directly like core/base's tests do.
foreach part : [
  'common',
  'vec',
  'x87',
]
  test(
    'core/math/' + part,
    executable(
      'test_' + part,
      'test_' + part + '.c',
      c_args: core_include,
      implicit_include_directories: false,
      link_with: [core_math_lib, core_base_lib],
      dependencies: [cc_libm],
    ),
    suite: 'unit',
  )
endforeach

# The x87 comparison test only builds on x86 with a compiler that supports
# GCC's inline asm; test_x87_asm.c itself is a no-op test elsewhere.
test(
  'core/math/x87_asm',
  executable(
    'test_x87_asm',
    'test_x87_asm.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_math_lib, core_base_lib],
    dependencies: [cc_libm],
  ),
  suite: 'unit',
)

test(
  'core/config',
  executable(
    'test_config',
    'test_config.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_config_lib, core_ini_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)

test(
  'core/image/pixels',
  executable(
    'test_image_pixels',
    'test_image_pixels.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_image_lib, core_base_lib],
    dependencies: [avcodec, avutil, swscale, cc_libm],
  ),
  suite: 'unit',
)

test(
  'core/image/codec',
  executable(
    'test_image_codec',
    'test_image_codec.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_image_lib, core_io_lib, core_base_lib, core_hash_lib],
    dependencies: [avcodec, avutil, swscale, cc_libm],
  ),
  suite: 'unit',
)

test(
  'core/ini',
  executable(
    'test_ini',
    'test_ini.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_ini_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'ini',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'ini',
  },
  suite: 'unit',
)

test(
  'core/input',
  executable(
    'test_input',
    'test_input.c',
    'input_backend_fake.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [
      core_input_lib, core_config_lib, core_ini_lib, core_io_lib,
      core_base_lib,
    ],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)

test(
  'shell/game/request_queue',
  executable(
    'test_request_queue',
    'test_request_queue.c',
    'input_backend_fake.c',
    '../src/shell/game/request_queue.c',
    c_args: shell_include,
    implicit_include_directories: false,
    link_with: [
      core_input_lib, core_config_lib, core_ini_lib, core_io_lib,
      core_base_lib,
    ],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)

test(
  'core/patch/files',
  executable(
    'test_patch_files',
    'test_patch_files.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [
      core_patch_lib, core_json_lib, core_vfs_lib, core_io_lib, core_base_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {'TEST_DIR': meson.current_build_dir() / 'tmp' / 'patch_files'},
  suite: 'unit',
)

test(
  'core/patch/strings',
  executable(
    'test_patch_strings',
    'test_patch_strings.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [
      core_patch_lib, core_json_lib, core_vfs_lib, core_io_lib, core_base_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {'TEST_DIR': meson.current_build_dir() / 'tmp' / 'patch_strings'},
  suite: 'unit',
)

# The tests of the manager read repository networks from their own
# directories with a fetch function that reads files. The manager allocates
# its arena with malloc, which MALLOC_PERTURB_ would fill byte by byte, so
# the tests turn it off.
foreach part : ['language', 'manager', 'repo', 'select', 'update']
  test(
    'core/patch/' + part,
    executable(
      'test_patch_' + part,
      'test_patch_' + part + '.c',
      c_args: core_include,
      implicit_include_directories: false,
      link_with: [
        core_patch_select_lib, core_ini_lib, core_json_lib, core_io_lib,
        core_hash_lib, core_base_lib,
      ],
      dependencies: [sdl3],
    ),
    env: {
      'TEST_DIR': meson.current_build_dir() / 'tmp' / 'patch_' + part,
      'MALLOC_PERTURB_': '0',
    },
    suite: 'unit',
  )
endforeach

test(
  'core/patch/stack',
  executable(
    'test_patch_stack',
    'test_patch_stack.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [
      core_patch_lib, core_json_lib, core_vfs_lib, core_image_lib, core_io_lib,
      core_base_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'patch_stack',
    'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
  },
  suite: 'unit',
)

test(
  'core/vfs',
  executable(
    'test_vfs',
    'test_vfs.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_vfs_lib, core_io_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'core/archive',
  executable(
    'test_archive',
    'test_archive.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_archive_lib, core_vfs_lib, core_io_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'stg/lzss',
  executable(
    'test_lzss',
    'test_lzss.c',
    'pbg3_builder.c',
    c_args: stg_include,
    implicit_include_directories: false,
    link_with: [stg_lzss_lib, core_hash_lib, core_io_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'stg/pbg3',
  executable(
    'test_pbg3',
    'test_pbg3.c',
    'pbg3_builder.c',
    c_args: stg_include,
    implicit_include_directories: false,
    link_with: [
      stg_pbg3_lib, core_archive_lib, core_vfs_lib, core_io_lib, core_base_lib,
    ],
  ),
  suite: 'unit',
)

test(
  'stg/pbg4',
  executable(
    'test_pbg4',
    'test_pbg4.c',
    c_args: stg_include,
    implicit_include_directories: false,
    link_with: [
      stg_pbg4_lib, core_archive_lib, core_vfs_lib, core_io_lib, core_base_lib,
    ],
  ),
  suite: 'unit',
)

test(
  'core/vfs/mount_dir',
  executable(
    'test_vfs_mount_dir',
    'test_vfs_mount_dir.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_vfs_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'vfs_mount_dir',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'vfs_mount_dir',
  },
  suite: 'unit',
)

test(
  'core/input/duplicate_action',
  python,
  args: [
    expect_fatal,
    'input: action "shoot" is in two schemas',
    executable(
      'test_input_duplicate_action',
      'test_input_duplicate_action.c',
      'input_backend_fake.c',
      c_args: core_include,
      implicit_include_directories: false,
      link_with: [
        core_input_lib, core_config_lib, core_ini_lib, core_io_lib,
        core_base_lib,
      ],
      dependencies: [sdl3],
    ),
  ],
  suite: 'unit',
)

# Each of these files compiles, and fails to compile with BREAK defined,
# which shows that the container macros reject a value of the wrong type.
compile_fail_args = core_include + ['-Werror']
foreach name : [
  'array_item_type',
  'array_slice_type',
  'config_bool_on_float_field',
  'config_float_on_int_field',
  'hash_map_key_type',
  'hash_map_value_type',
  'input_button_on_float_field',
  'input_stick_on_bool_field',
  'input_stick_with_gap',
]
  source = files('compile_fail' / name + '.c')
  if not cc.compiles(source, args: compile_fail_args, name: name)
    error('tests/compile_fail/' + name + '.c does not compile')
  endif
  if cc.compiles(source, args: compile_fail_args + ['-DBREAK'], name: name + ' with BREAK')
    error('tests/compile_fail/' + name + '.c compiles with BREAK')
  endif
endforeach

test(
  'core/gfx',
  executable(
    'test_gfx',
    'test_gfx.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_gfx_lib, core_image_lib, core_math_lib, core_base_lib],
    dependencies: [cc_libm],
  ),
  suite: 'unit',
)

test(
  'core/gfx/duplicate_id',
  python,
  args: [
    expect_fatal,
    'Assertion failed: !duplicate',
    executable(
      'test_gfx_duplicate_id',
      'test_gfx_duplicate_id.c',
      c_args: core_include,
      implicit_include_directories: false,
      link_with: [core_gfx_lib, core_image_lib, core_math_lib, core_base_lib],
      dependencies: [cc_libm],
    ),
  ],
  suite: 'unit',
)

# The pixel tests need an OpenGL driver, so they are built here and run
# outside the Docker image (see docs/BUILDING.md).
executable(
  'test_gfx_gl',
  'test_gfx_gl.c',
  c_args: core_include,
  implicit_include_directories: false,
  link_with: [
    core_gfx_gl_lib, core_gfx_lib, core_image_lib, core_math_lib,
    core_base_lib,
  ],
  dependencies: [sdl3, avcodec, avutil, swscale, cc_libm],
)


# The text tests run against IPAGothic, and again against MS Gothic
# where $OPENTOUHOU_MSGOTHIC names a msgothic.ttc; without it, the second
# test is skipped.
foreach variant : [['core/text', 'test_text', []], ['core/text/ms_gothic', 'test_text_ms', ['-DTEST_MS_GOTHIC']]]
  test(
    variant[0],
    executable(
      variant[1],
      'test_text.c',
      c_args: core_include + variant[2],
      implicit_include_directories: false,
      link_with: [
        core_patch_lib, core_json_lib, core_vfs_lib, core_text_lib,
        core_image_lib, core_io_lib, core_base_lib,
      ],
      dependencies: [sdl3_ttf, sdl3, text_system, cc_libm],
    ),
    suite: 'unit',
    env: {
      'TEST_DIR': meson.current_build_dir() / 'tmp' / variant[1],
      'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
    },
  )
endforeach

# The test stands in for the system's fonts, so it links no backend of
# them.
test(
  'core/text/font_registry',
  executable(
    'test_font_registry',
    'test_font_registry.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_text_lib, core_image_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3_ttf, sdl3, cc_libm],
  ),
  suite: 'unit',
  env: {
    'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
  },
)

test(
  'core/text/font_rom',
  executable(
    'test_font_rom',
    'test_font_rom.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_text_lib, core_image_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3_ttf, sdl3, text_system, cc_libm],
  ),
  suite: 'unit',
)

test(
  'shell/game/fonts',
  executable(
    'test_shell_fonts',
    'test_shell_fonts.c',
    c_args: shell_include + [
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
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'shell_fonts',
    'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
  },
)

test(
  'th07/effect',
  executable(
    'test_th07_effect',
    'test_th07_effect.c',
    'th07_anm_builder.c',
    'th07_data.c',
    'th07_scene_stubs.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_effect',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th07_effect',
  },
  suite: 'unit',
)

test(
  'th07/player_shot_type',
  executable(
    'test_th07_player_shot_type',
    'test_th07_player_shot_type.c',
    shell_fake,
    'th07_data.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_player_shot_type',
  },
  suite: 'unit',
)

test(
  'th07/bomb',
  executable(
    'test_th07_bomb',
    'test_th07_bomb.c',
    'th07_anm_builder.c',
    'th07_data.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_bomb',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th07_bomb',
  },
  suite: 'unit',
)

test(
  'th07/item',
  executable(
    'test_th07_item',
    'test_th07_item.c',
    'th07_anm_builder.c',
    'th07_data.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_item',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th07_item',
  },
  suite: 'unit',
)

test(
  'th07/enemy',
  executable(
    'test_th07_enemy',
    'test_th07_enemy.c',
    'th07_anm_builder.c',
    'th07_data.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_enemy',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th07_enemy',
  },
  suite: 'unit',
)

test(
  'th07/ecl',
  executable(
    'test_th07_ecl',
    'test_th07_ecl.c',
    shell_fake,
    'th07_ecl_builder.c',
    'th07_play_setup.c',
    'th07_anm_builder.c',
    'th07_data.c',
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_ecl',
  },
  suite: 'unit',
)

test(
  'th07/ecl_patterns',
  executable(
    'test_th07_ecl_patterns',
    'test_th07_ecl_patterns.c',
    shell_fake,
    'th07_play_setup.c',
    'th07_anm_builder.c',
    'th07_data.c',
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_ecl_patterns',
  },
  suite: 'unit',
)

test(
  'th07/player',
  executable(
    'test_th07_player',
    'test_th07_player.c',
    'th07_anm_builder.c',
    'th07_data.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_player',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th07_player',
  },
  suite: 'unit',
)

test(
  'th07/bullet',
  executable(
    'test_th07_bullet',
    'test_th07_bullet.c',
    shell_fake,
    'th07_anm_builder.c',
    'th07_data.c',
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th07_bullet',
  },
  suite: 'unit',
)

test(
  'th07/player_shots',
  executable(
    'test_th07_player_shots',
    'test_th07_player_shots.c',
    shell_fake,
    'gfx_recorder.c',
    c_args: [layer_include['th07']] + core_include + stg_include + shell_include,
    implicit_include_directories: false,
    link_with: [
      th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib,
      stg_pbg4_lib, stg_lzss_lib, core_patch_lib, core_json_lib,
      core_vfs_lib, core_archive_lib, core_io_lib, core_base_lib,
      core_gfx_lib, core_image_lib, core_math_lib,
    ],
    dependencies: [sdl3],
  ),
  suite: 'unit',
)
