# The tests of core/base, core/args, core/hash, core/json, core/io,
# core/archive and the tools are in the suite core or tools. Every build
# runs them, whatever programs it has.

# The tests link the external libraries from one shared library instead of
# each carrying its own copy. Each external library's variable here links
# that library as well, and a test that reaches an external library only
# through the libraries that it links names test_deps. FreeType's archive
# hides its symbols, so the tests link FreeType statically. The libraries
# need libm, which the external libraries' flags otherwise bring.
test_deps_archives = []
test_deps_static = [cc_libm]
foreach name, lib : external_libs
  if name == 'freetype'
    test_deps_static += lib[0]
  else
    test_deps_archives += '@0@/lib@1@.a'.format(lib[0].get_variable(pkgconfig: 'libdir'), lib[1])
  endif
endforeach
if host_machine.system() == 'darwin'
  test_deps_args = []
  foreach archive : test_deps_archives
    test_deps_args += '-Wl,-force_load,' + archive
  endforeach
else
  test_deps_args = ['-Wl,--whole-archive'] + test_deps_archives + ['-Wl,--no-whole-archive']
endif
if host_machine.system() == 'windows'
  # MinGW otherwise exports only the marked symbols when an object marks any.
  test_deps_args += '-Wl,--export-all-symbols'
elif host_machine.system() == 'linux'
  # swscale's x86 inline assembly addresses its tables directly, which a
  # shared library allows only when it binds its own symbols.
  test_deps_args += '-Wl,-Bsymbolic'
endif
test_deps = declare_dependency(
  link_with: shared_library('test_deps', link_args: test_deps_args, dependencies: external_deps),
  dependencies: test_deps_static,
)
foreach name, lib : external_libs
  set_variable(name, declare_dependency(dependencies: [get_variable(name), test_deps]))
endforeach

node = find_program('node', required: false)
if node.found()
  test(
    'shell/web/data',
    node,
    args: ['--test', files('web/test_data.cjs', 'web/test_audio.cjs')],
    suite: ['unit', 'tools'],
  )
endif

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
  'core/archive/hdi',
  executable(
    'test_archive_hdi',
    'test_archive_hdi.c',
    c_args: core_include,
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
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_archive_lib, core_hash_lib, core_io_lib, core_base_lib],
    dependencies: [zlib, sdl3],
  ),
  env: {'TEST_DIR': meson.current_build_dir() / 'tmp' / 'archive_zip'},
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
        dependencies: othpatch_deps + [test_deps],
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

foreach part : ['audio', 'audio_decoder', 'audio_resampler', 'audio_live']
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

test(
  'core/audio/zip',
  executable(
    'test_audio_zip',
    'test_audio_zip.c',
    'audio_backend_fake.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_audio_lib, core_archive_lib, core_hash_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3, zlib, avcodec, avutil, avformat, swresample, cc_libm],
  ),
  suite: 'unit',
)

# The tests of th06 and th07. Each runs test_<part>.c, part being its
# name with underscores for slashes unless it gives its own, with the
# extra sources that it lists. The variables in env point TEST_DIR and
# XDG_DATA_HOME to its directory under tmp, named after dir or part, and
# TEST_FONT_DIR to the built-in fonts.
th06_data = ['gfx_recorder.c', 'anm_builder.c', 'pbg3_builder.c', 'th06_data.c']
th07_data = ['th07_anm_builder.c', 'th07_data.c', 'gfx_recorder.c']
game_test_libs = {
  'th06': [
    th06_lib, stg_translation_lib, stg_note_lib, stg_end_diff_lib, stg_rng_lib, stg_pbg3_lib,
    core_patch_lib, core_json_lib, core_vfs_lib, core_audio_lib, core_config_lib, core_ini_lib,
    core_io_lib, core_math_lib, core_base_lib, core_gfx_lib, core_image_lib,
  ],
  'th07': [
    th07_lib, stg_chain_lib, stg_angle_lib, stg_timer_lib, stg_rng_lib, stg_pbg4_lib, stg_lzss_lib,
    stg_end_diff_lib, core_patch_lib, core_json_lib, core_vfs_lib, core_archive_lib, core_audio_lib,
    core_config_lib, core_ini_lib, core_io_lib, core_math_lib, core_base_lib, core_gfx_lib,
    core_image_lib,
  ],
}
test_fonts = meson.project_source_root() / 'src' / 'shell' / 'builtin' / 'fonts'
foreach game, game_tests : {
  'th06': {
    'th06/ascii': {'sources': ['gfx_recorder.c'], 'deps': [test_deps]},
    'th06/ascii_menu': {'sources': ['gfx_recorder.c'], 'deps': [test_deps]},
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
    'th06/screen_effect': {'sources': ['gfx_recorder.c'], 'deps': [test_deps]},
    'th06/sound': {
      'sources': ['audio_backend_fake.c', 'pbg3_builder.c', 'th06_data.c'],
      'deps': [sdl3, avcodec, avutil, avformat, swresample, cc_libm],
      'env': ['TEST_DIR'],
    },
    'th06/stage': {'sources': th06_data, 'env': ['TEST_DIR', 'XDG_DATA_HOME']},
    'th06/ecl': {
      'sources': ['anm_builder.c', 'ecl_builder.c', 'pbg3_builder.c', 'th06_data.c', 'th06_scene_stubs.c'],
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
      'sources': ['pbg3_builder.c', 'th06_data.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
  },
  'th07': {
    'th07/buttons': {'deps': [test_deps]},
    'th07/gui': {
      'sources': th07_data + ['th07_gui_setup.c', 'th07_msg_builder.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    # The _ms tests run again against MS Gothic where $OPENTOUHOU_MSGOTHIC
    # names a msgothic.ttc.
    'th07/gui/text_image': {
      'part': 'th07_text_image',
      'sources': th07_data + ['../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME', 'TEST_FONT_DIR'],
      'dir': 'test_th07_text_image',
    },
    'th07/gui/text_image/ms_gothic': {
      'part': 'th07_text_image_ms',
      'main': 'test_th07_text_image.c',
      'sources': th07_data + ['../src/th07/play_time.c'],
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
    'th07/play_time': {'sources': ['../src/th07/play_time.c'], 'deps': [test_deps]},
    'th07/scene': {
      'sources': th07_data + ['../src/th07/play_time.c'],
      'env': ['TEST_DIR'],
    },
    'th07/anm': {
      'sources': th07_data + ['../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th07/stage': {
      'sources': th07_data + ['../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th07/anm/vm': {'sources': ['../src/th07/play_time.c']},
    'th07/anm/draw': {
      'sources': ['../src/th07/play_time.c', 'gfx_recorder.c'],
      'env': ['TEST_FONT_DIR'],
    },
    'th07/anm/text': {
      'sources': ['../src/th07/play_time.c', 'gfx_recorder.c'],
      'env': ['TEST_FONT_DIR'],
    },
    'th07/anm/text/ms_gothic': {
      'part': 'th07_anm_text_ms',
      'main': 'test_th07_anm_text.c',
      'sources': ['../src/th07/play_time.c', 'gfx_recorder.c'],
      'c_args': ['-DTEST_MS_GOTHIC'],
      'env': ['TEST_FONT_DIR'],
    },
    'th07/screen_effect': {'sources': ['../src/th07/play_time.c', 'gfx_recorder.c']},
    'th07/screens/title': {
      'part': 'th07_title',
      'sources': th07_data + ['../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'TEST_FONT_DIR'],
    },
    'th07/ascii': {
      'sources': th07_data + ['../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th07/ascii_menu': {
      'sources': th07_data + ['../src/th07/play_time.c'],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th07/sound': {
      'sources': ['audio_backend_fake.c', 'th07_data.c', '../src/th07/play_time.c'],
      'deps': [sdl3, avcodec, avutil, avformat, swresample, cc_libm],
      'env': ['TEST_DIR', 'XDG_DATA_HOME'],
    },
    'th07/files': {'sources': ['th07_data.c'], 'env': ['TEST_DIR', 'XDG_DATA_HOME']},
    'th07/effect': {
      'sources': th07_data + ['../src/th07/play_time.c'],
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
    'th07/player_shots': {'sources': ['gfx_recorder.c']},
  },
}
  foreach name, t : game_tests
    part = t.get('part', name.replace('/', '_'))
    env = {}
    foreach var : t.get('env', [])
      env += {var: var == 'TEST_FONT_DIR' ? test_fonts : meson.current_build_dir() / 'tmp' / t.get('dir', part)}
    endforeach
    test(
      name,
      executable(
        'test_' + part,
        t.get('main', 'test_' + part + '.c'),
        t.get('sources', []),
        shell_fake,
        c_args: [layer_include[game]] + t.get('c_args', []) + core_include + stg_include + shell_include,
        implicit_include_directories: false,
        link_with: game_test_libs[game],
        dependencies: t.get('deps', [sdl3]),
      ),
      env: env,
      suite: 'unit',
    )
  endforeach
endforeach

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
  'stg/replay_files',
  executable(
    'test_stg_replay_files',
    'test_stg_replay_files.c',
    c_args: stg_include,
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
    c_args: stg_include,
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
    c_args: stg_include,
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
    c_args: core_include + stg_include + shell_include + bot_include,
    implicit_include_directories: false,
    link_with: bot_lib,
    dependencies: [core_dep, stg_dep, sdl3, cc_libm],
  ),
  env: { 'TEST_DIR': meson.current_build_dir() / 'tmp' / 'recorder' },
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

test(
  'replay/dosbox',
  executable(
    'test_dosbox',
    'test_dosbox.c',
    c_args: core_include + harness_include + harness_dosbox_include,
    implicit_include_directories: false,
    link_with: [harness_dosbox_lib, harness_lib],
    dependencies: [sdl3, zlib, avcodec, avutil, swscale, jpeg, cc_libm],
  ),
  env: { 'TEST_DIR': meson.current_build_dir() / 'tmp' / 'dosbox' },
  suite: 'unit',
)

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
  'shell/data',
  executable(
    'test_shell_data',
    'test_shell_data.c',
    '../src/shell/data.c',
    c_args: shell_include,
    implicit_include_directories: false,
    link_with: [
      core_vfs_lib, core_archive_lib, core_hash_lib, core_io_lib,
      core_base_lib,
    ],
    dependencies: [zlib, sdl3],
  ),
  env: {'TEST_DIR': meson.current_build_dir() / 'tmp' / 'shell_data'},
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
    dependencies: [test_deps],
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
    dependencies: [test_deps],
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
    dependencies: [test_deps],
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
    dependencies: [test_deps],
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
    dependencies: [test_deps],
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

if build_games
  test(
    'th01/entry',
    executable(
      'test_th01_entry',
      'test_th01_entry.c',
      '../src/th01/entry.c',
      c_args: [layer_include['th01'], layer_include['pc98'], '-DSDL_MAIN_HANDLED'] + shell_include,
      implicit_include_directories: false,
      dependencies: [sdl3],
    ),
    suite: 'unit',
  )
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
      th01_boss_fixtures,
      'audio_backend_fake.c',
      c_args: [layer_include['th01']] + shell_include + stg_include,
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
      c_args: [layer_include['th01']] + core_include + stg_include,
      implicit_include_directories: false,
      link_with: [th01_lib, core_text_lib, core_image_lib, core_io_lib, core_base_lib],
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
      'input_backend_fake.c',
      c_args: [layer_include['th01']] + core_include + stg_include,
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
      'input_backend_fake.c',
      c_args: [layer_include['th01']] + core_include + stg_include,
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
      'audio_backend_fake.c',
      c_args: [layer_include['th01']] + core_include + stg_include,
      implicit_include_directories: false,
      link_with: [th01_lib, core_audio_lib, core_io_lib, core_base_lib],
      dependencies: [pc98_dep, sdl3, avcodec, avutil, avformat, swresample, cc_libm],
    ),
    suite: 'unit',
  )
  foreach part : ['bos', 'boss_b05', 'boss_b10m', 'boss_b10j', 'boss_b15j', 'boss_b15m', 'boss_b20j', 'boss_b20m', 'boss_entity', 'boss_render', 'boss_hit', 'boss_defeat', 'route_select', 'sprite', 'shape', 'particles', 'wave', 'interleave', 'transition', 'scale2x', 'hud', 'hud_hp', 'extend', 'timer', 'items', 'bonus', 'shots', 'pellet_assets', 'shootout_lasers', 'pellets', 'missiles', 'line_bullets', 'player', 'bomb', 'miss', 'vsync_irq', 'wait', 'finite', 'orb', 'settings', 'score_data', 'score', 'ending', 'session', 'invincibility', 'gameplay', 'video', 'palette', 'tram', 'ptn', 'grc', 'grf', 'grz', 'stage_data', 'scene', 'stage_objects', 'stage', 'stage_obstacles']
    test(
      'th01/' + part,
      executable(
        'test_th01_' + part,
        'test_th01_' + part + '.c',
        part == 'gameplay' ? ['audio_backend_fake.c', 'th01_assets.c'] + th01_boss_fixtures : [],
        c_args: [layer_include['th01']] + core_include + stg_include,
        implicit_include_directories: false,
        link_with: [th01_lib, core_hash_lib, core_config_lib, core_ini_lib, core_vfs_lib, core_image_lib, core_io_lib, core_base_lib] + (part in ['gameplay', 'score', 'ending'] ? [core_text_lib] : []) + (part == 'gameplay' ? [core_audio_lib] : []),
        dependencies: [pc98_dep, sdl3] + (part == 'gameplay' ? [avcodec, avutil, avformat, swresample, cc_libm] : []),
      ),
      suite: 'unit',
      env: {
        'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th01' / part,
        'TEST_SPRITE_DIR': meson.project_source_root() / 'src' / 'th01' / 'sprites',
      },
    )
  endforeach
  test(
    'replay/th01',
    executable(
      'test_th01_harness',
      'test_th01_harness.c',
      c_args: [layer_include['th01']] + core_include + stg_include + harness_include + ['-I' + meson.project_source_root() / 'replay/th01'],
      implicit_include_directories: false,
      link_with: [th01_harness_lib, th01_lib, core_hash_lib, core_config_lib, core_ini_lib, core_vfs_lib, core_image_lib, core_io_lib, core_base_lib],
      dependencies: [pc98_dep, sdl3],
    ),
    suite: 'unit',
    env: {'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th01' / 'harness'},
  )
endif

foreach part : ['mdrv_data', 'mdrv', 'pmd', 'opna', 'vsync', 'master_rng', 'math16', 'bfnt', 'logo1', 'pf', 'pi', 'sprite_sheet', 'pmd_data', 'pmd_effect', 'pmd_effect_stream', 'pmd_fm', 'pmd_lfo', 'pmd_part', 'pmd_pitch', 'pmd_ssg', 'pmd_drums', 'pmd_sequence']
  test(
    'pc98/' + part,
    executable(
      'test_pc98_' + part,
      'test_pc98_' + part + '.c',
      implicit_include_directories: false,
      dependencies: [pc98_dep, test_deps] + (part in ['mdrv_data', 'sprite_sheet', 'pmd_data'] ? [sdl3] : []),
    ),
    suite: 'unit',
    env: {'TEST_DIR': meson.current_build_dir() / 'tmp' / 'pc98' / part},
  )
endforeach
test(
  'pc98/mdrv_malformed',
  python,
  args: [
    expect_fatal,
    'Invalid MDRV opcode d0 at 0',
    executable(
      'test_pc98_mdrv_malformed',
      'test_pc98_mdrv_malformed.c',
      implicit_include_directories: false,
      dependencies: [pc98_dep, sdl3],
    ),
  ],
  suite: 'unit',
)

test(
  'core/text/font_rom_cells',
  executable(
    'test_font_rom_cells',
    'test_font_rom_cells.c',
    c_args: core_include,
    implicit_include_directories: false,
    link_with: [core_text_lib, core_image_lib, core_io_lib, core_base_lib],
    dependencies: [sdl3_ttf, sdl3, text_system, cc_libm],
  ),
  suite: 'unit',
)

executable(
  'test_shell_clocks',
  'test_shell_clocks.c',
  c_args: shell_include,
  implicit_include_directories: false,
  link_with: builtin_libs['th06'],
  dependencies: [shell_game_dep, sdl3],
)

executable(
  'test_shell_save_settings',
  'test_shell_save_settings.c',
  c_args: shell_include,
  implicit_include_directories: false,
  link_with: builtin_libs['th06'],
  dependencies: [shell_game_dep, sdl3],
)

if build_games
  foreach part : ['input', 'tram', 'grcg', 'clip', 'video', 'tiny', 'patterns', 'performance']
    test(
      'th02/' + part,
      executable(
        'test_th02_' + part,
        'test_th02_' + part + '.c',
        c_args: [layer_include['th02'], layer_include['pc98']] + core_include,
        implicit_include_directories: false,
        link_with: [th02_lib, core_base_lib],
        dependencies: [sdl3],
      ),
      suite: 'unit',
    )
  endforeach
  foreach part : ['wait']
    test(
      'th02/' + part,
      executable(
        'test_th02_' + part,
        'test_th02_' + part + '.c',
        c_args: [layer_include['th02'], layer_include['pc98']] + core_include,
        implicit_include_directories: false,
        link_with: [th02_lib, core_base_lib],
        dependencies: [pc98_dep, sdl3],
      ),
      suite: 'unit',
    )
  endforeach
  foreach part : ['tram_text', 'gaiji', 'palette', 'map', 'mpn', 'scores']
    test(
      'th02/' + part,
      executable(
        'test_th02_' + part,
        'test_th02_' + part + '.c',
        c_args: [layer_include['th02'], layer_include['pc98']] + core_include,
        implicit_include_directories: false,
        link_with: [th02_lib, core_io_lib, core_base_lib],
        dependencies: [sdl3],
      ),
      suite: 'unit',
    )
  endforeach
  test(
    'th02/settings',
    executable(
      'test_th02_settings',
      'test_th02_settings.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include,
      implicit_include_directories: false,
      link_with: [th02_lib, core_config_lib, core_ini_lib, core_vfs_lib,
        core_io_lib, core_base_lib],
      dependencies: [sdl3],
    ),
    suite: 'unit',
    env: {'TEST_DIR': meson.current_build_dir() / 'tmp' / 'th02' / 'settings'},
  )
  test(
    'th02/demo',
    executable(
      'test_th02_demo',
      'test_th02_demo.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include,
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
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include,
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
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include,
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
      'audio_backend_fake.c',
      c_args: [layer_include['th02'], layer_include['pc98']] + core_include,
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
      implicit_include_directories: false,
      dependencies: [pc98_dep, test_deps],
    ),
    suite: 'unit',
  )
endif
