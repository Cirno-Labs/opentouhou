foreach ascii_test : ['ascii', 'ascii_menu']
  test(
    'th06/' + ascii_test,
    executable(
      'test_th06_' + ascii_test,
      'test_th06_' + ascii_test + '.c',
      c_args: [layer_include['th06']] + core_include + stg_include,
      implicit_include_directories: false,
      link_with: [th06_lib, core_base_lib],
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
      c_args: [layer_include['th06']] + core_include + stg_include,
      implicit_include_directories: false,
      link_with: [th06_lib, stg_rng_lib, core_io_lib, core_base_lib],
    ),
    suite: 'unit',
  )
endforeach

test(
  'th06/game_manager/chain',
  executable(
    'test_th06_game_manager_chain',
    'test_th06_game_manager_chain.c',
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    link_with: [th06_lib, stg_rng_lib, core_io_lib, core_math_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'th06/angle',
  executable(
    'test_th06_angle',
    'test_th06_angle.c',
    c_args: [layer_include['th06']] + core_include,
    implicit_include_directories: false,
    link_with: [th06_lib, core_math_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'th06/anm',
  executable(
    'test_th06_anm',
    'test_th06_anm.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    c_args: [layer_include['th06']] + stg_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_base_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_anm',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_anm',
  },
  suite: 'unit',
)

test(
  'th06/buttons',
  executable(
    'test_th06_buttons',
    'test_th06_buttons.c',
    c_args: [layer_include['th06']] + core_include,
    implicit_include_directories: false,
    link_with: [th06_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'th06/chain',
  executable(
    'test_th06_chain',
    'test_th06_chain.c',
    c_args: [layer_include['th06']] + core_include,
    implicit_include_directories: false,
    link_with: [th06_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'th06/game_manager',
  executable(
    'test_th06_game_manager',
    'test_th06_game_manager.c',
    c_args: [layer_include['th06']] + core_include,
    implicit_include_directories: false,
    link_with: [th06_lib],
  ),
  suite: 'unit',
)

test(
  'th06/bomb',
  executable(
    'test_th06_bomb',
    'test_th06_bomb.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib,
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
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib,
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
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib,
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
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib,
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
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    link_with: [th06_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'th06/screen_effect',
  executable(
    'test_th06_screen_effect',
    'test_th06_screen_effect.c',
    'th06_scene_stubs.c',
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    link_with: [th06_lib, stg_rng_lib, core_math_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'th06/stage',
  executable(
    'test_th06_stage',
    'test_th06_stage.c',
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib,
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
  'th06/timer',
  executable(
    'test_th06_timer',
    'test_th06_timer.c',
    c_args: [layer_include['th06']] + core_include,
    implicit_include_directories: false,
    link_with: [th06_lib],
  ),
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
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib,
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
    'anm_builder.c',
    'ecl_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib,
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
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib,
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
    'anm_builder.c',
    'pbg3_builder.c',
    'th06_data.c',
    'th06_scene_stubs.c',
    c_args: [layer_include['th06']] + core_include + stg_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_rng_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib,
      core_math_lib, core_base_lib,
    ],
    dependencies: [sdl3],
  ),
  env: {
    'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th06_gui',
    'XDG_DATA_HOME': meson.current_build_dir() / 'tmp' / 'th06_gui',
  },
  suite: 'unit',
)

test(
  'th06/files',
  executable(
    'test_th06_files',
    'test_th06_files.c',
    'pbg3_builder.c',
    'th06_data.c',
    c_args: [layer_include['th06']] + core_include,
    implicit_include_directories: false,
    link_with: [
      th06_lib, stg_pbg3_lib, core_vfs_lib, core_io_lib, core_base_lib,
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

# The replay test case format lives in replay/core, outside the game layers.
replay_include = ['-I' + meson.current_source_dir() / '../replay/core']

replay_case_lib = static_library(
  'replay_case',
  '../replay/core/case.c',
  c_args: core_include + replay_include,
  implicit_include_directories: false,
  dependencies: [zlib],
)

test(
  'replay/core/case',
  executable(
    'test_case',
    'test_case.c',
    c_args: core_include + replay_include,
    implicit_include_directories: false,
    link_with: [replay_case_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3, zlib],
  ),
  suite: 'unit',
)

# The tests include core's private headers, so they compile against the
# sources directly instead of the library.
test(
  'core/app/clock',
  executable(
    'test_clock',
    'test_clock.c',
    '../src/core/app/clock.c',
    c_args: core_include,
    implicit_include_directories: false,
  ),
  suite: 'unit',
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
    link_with: [core_image_lib, core_io_lib, core_base_lib],
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
  'stg/pbg3',
  executable(
    'test_pbg3',
    'test_pbg3.c',
    'pbg3_builder.c',
    c_args: stg_include,
    implicit_include_directories: false,
    link_with: [stg_pbg3_lib, core_vfs_lib, core_io_lib, core_base_lib],
  ),
  suite: 'unit',
)

test(
  'core/vfs/mount_file',
  executable(
    'test_vfs_mount_file',
    'test_vfs_mount_file.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_vfs_lib, core_io_lib, core_base_lib],
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

# Each part of core/base has a test named after it.
foreach part : [
  'arena',
  'array',
  'cp932',
  'hash_map',
  'log',
  'memory',
  'result',
  'sha1',
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
    suite: 'unit',
  )
endforeach

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
    suite: 'unit',
  )
endforeach

# Each part of core/io has tests named after it. A test that works with
# files does so in its own directory inside the build directory, which is
# also its user directory.
foreach name : [
  'file',
  'path',
  'reader',
  'reader_file',
  'reader_owned_buffer',
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
    suite: 'unit',
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
    suite: 'unit',
  )
endforeach

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
