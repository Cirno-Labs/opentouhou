# The tests of core/base, core/args, core/hash, core/json, core/io and
# core/archive are in the suite core. Every build runs them, whatever
# programs it has; the rest of core needs the libraries of the games, the
# same guard tests/meson.build uses before it reaches this subdir's
# callers.

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
      c_args: core_include + tests_include,
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
      c_args: core_include + tests_include,
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
    c_args: core_include + tests_include,
    implicit_include_directories: false,
    link_with: [core_args_lib, core_base_lib],
  ),
  suite: ['unit', 'core'],
)

test(
  'core/archive/hdi',
  executable(
    'test_archive_hdi',
    'test_archive_hdi.c',
    c_args: core_include + tests_include,
    implicit_include_directories: false,
    link_with: [core_archive_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3],
  ),
  suite: ['unit', 'core'],
)

test(
  'core/archive/zip',
  executable(
    'test_archive_zip',
    'test_archive_zip.c',
    c_args: core_include + tests_include,
    implicit_include_directories: false,
    link_with: [core_archive_lib, core_hash_lib, core_io_lib, core_base_lib],
    dependencies: [zlib, sdl3],
  ),
  env: {'TEST_DIR': tests_tmp / 'archive_zip'},
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
      c_args: core_include + tests_include,
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
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [core_io_lib, core_base_lib],
      dependencies: [sdl3],
    ),
    env: {
      'TEST_DIR': tests_tmp / name,
      'XDG_DATA_HOME': tests_tmp / name,
    },
    suite: ['unit', 'core'],
  )
endforeach

# These tests do something fatal. Each passes only when the program
# aborts with a fatal error that contains the given text.
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
        c_args: core_include + tests_include,
        implicit_include_directories: false,
        link_with: [core_io_lib, core_base_lib],
        dependencies: [test_deps],
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
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [core_args_lib, core_base_lib],
    ),
  ],
  suite: ['unit', 'core'],
)

if build_games
  test(
    'core/audio/device',
    executable(
      'test_audio_device',
      'test_audio_device.c',
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [core_audio_lib, core_io_lib, core_base_lib],
      dependencies: [sdl3, avcodec, avutil, avformat, swresample, cc_libm],
    ),
    env: {'SDL_AUDIO_DRIVER': 'dummy'},
    suite: 'unit',
  )

  foreach part : ['audio', 'audio_decoder', 'audio_resampler', 'audio_live']
    test(
      'core/' + part.replace('_', '/'),
      executable(
        'test_' + part,
        'test_' + part + '.c',
        '../audio_backend_fake.c',
        c_args: core_include + tests_include,
        implicit_include_directories: false,
        link_with: [core_audio_lib, core_io_lib, core_base_lib],
        dependencies: [sdl3, avcodec, avutil, avformat, swresample, cc_libm],
      ),
      suite: 'unit',
    )
  endforeach

  test(
    'core/audio/zip',
    executable(
      'test_audio_zip',
      'test_audio_zip.c',
      '../audio_backend_fake.c',
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [core_audio_lib, core_archive_lib, core_hash_lib, core_io_lib, core_base_lib],
      dependencies: [sdl3, zlib, avcodec, avutil, avformat, swresample, cc_libm],
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
        c_args: core_include + tests_include,
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
      c_args: core_include + tests_include,
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
      c_args: core_include + tests_include,
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
      c_args: core_include + tests_include,
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
      c_args: core_include + tests_include,
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
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [core_ini_lib, core_io_lib, core_base_lib],
      dependencies: [sdl3],
    ),
    env: {
      'TEST_DIR': tests_tmp / 'ini',
      'XDG_DATA_HOME': tests_tmp / 'ini',
    },
    suite: 'unit',
  )

  test(
    'core/input',
    executable(
      'test_input',
      'test_input.c',
      '../input_backend_fake.c',
      c_args: core_include + tests_include,
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
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [
        core_patch_lib, core_json_lib, core_vfs_lib, core_io_lib, core_base_lib,
      ],
      dependencies: [sdl3],
    ),
    env: {'TEST_DIR': tests_tmp / 'patch_files'},
    suite: 'unit',
  )

  test(
    'core/patch/strings',
    executable(
      'test_patch_strings',
      'test_patch_strings.c',
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [
        core_patch_lib, core_json_lib, core_vfs_lib, core_io_lib, core_base_lib,
      ],
      dependencies: [sdl3],
    ),
    env: {'TEST_DIR': tests_tmp / 'patch_strings'},
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
        c_args: core_include + tests_include,
        implicit_include_directories: false,
        link_with: [
          core_patch_select_lib, core_ini_lib, core_json_lib, core_io_lib,
          core_hash_lib, core_base_lib,
        ],
        dependencies: [sdl3],
      ),
      env: {
        'TEST_DIR': tests_tmp / 'patch_' + part,
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
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [
        core_patch_lib, core_json_lib, core_vfs_lib, core_image_lib, core_io_lib,
        core_base_lib,
      ],
      dependencies: [sdl3],
    ),
    env: {
      'TEST_DIR': tests_tmp / 'patch_stack',
      'TEST_FONT_DIR': meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts',
    },
    suite: 'unit',
  )

  test(
    'core/vfs',
    executable(
      'test_vfs',
      'test_vfs.c',
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [core_vfs_lib, core_io_lib, core_base_lib],
      dependencies: [test_deps],
    ),
    suite: 'unit',
  )

  test(
    'core/archive',
    executable(
      'test_archive',
      'test_archive.c',
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [core_archive_lib, core_vfs_lib, core_io_lib, core_base_lib],
      dependencies: [test_deps],
    ),
    suite: 'unit',
  )

  test(
    'core/vfs/mount_dir',
    executable(
      'test_vfs_mount_dir',
      'test_vfs_mount_dir.c',
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [core_vfs_lib, core_io_lib, core_base_lib],
      dependencies: [sdl3],
    ),
    env: {
      'TEST_DIR': tests_tmp / 'vfs_mount_dir',
      'XDG_DATA_HOME': tests_tmp / 'vfs_mount_dir',
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
        '../input_backend_fake.c',
        c_args: core_include + tests_include,
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
    source = files('../compile_fail' / name + '.c')
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
      c_args: core_include + tests_include,
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
        c_args: core_include + tests_include,
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
    c_args: core_include + tests_include,
    implicit_include_directories: false,
    link_with: [
      core_gfx_gl_lib, core_gfx_lib, core_image_lib, core_math_lib,
      core_base_lib,
    ],
    dependencies: [sdl3, avcodec, avutil, swscale, cc_libm],
  )

  foreach variant : [['core/text', 'test_text', []], ['core/text/ms_gothic', 'test_text_ms', ['-DTEST_MS_GOTHIC']]]
    test(
      variant[0],
      executable(
        variant[1],
        'test_text.c',
        c_args: core_include + variant[2] + tests_include,
        implicit_include_directories: false,
        link_with: [
          core_patch_lib, core_json_lib, core_vfs_lib, core_text_lib,
          core_image_lib, core_io_lib, core_base_lib,
        ],
        dependencies: [sdl3_ttf, sdl3, text_system, cc_libm],
      ),
      suite: 'unit',
      env: {
        'TEST_DIR': tests_tmp / variant[1],
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
      c_args: core_include + tests_include,
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
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [core_text_lib, core_image_lib, core_io_lib, core_base_lib],
      dependencies: [sdl3_ttf, sdl3, text_system, cc_libm],
    ),
    suite: 'unit',
  )

  test(
    'core/text/font_rom_cells',
    executable(
      'test_font_rom_cells',
      'test_font_rom_cells.c',
      c_args: core_include + tests_include,
      implicit_include_directories: false,
      link_with: [core_text_lib, core_image_lib, core_io_lib, core_base_lib],
      dependencies: [sdl3_ttf, sdl3, text_system, cc_libm],
    ),
    suite: 'unit',
  )
endif
