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
      implicit_include_directories: false,
      link_with: othpatch_libs + [test_lib]
        + (part in ['install', 'main'] ? [zip_builder_lib] : []),
      dependencies: othpatch_deps + [test_deps],
    ),
    env: {
      'TEST_DIR': tests_tmp / 'othpatch_' + part,
      'XDG_DATA_HOME': tests_tmp / 'othpatch_' + part,
      'MALLOC_PERTURB_': '0',
      'FAKE_PROGRAM': fake_program.full_path(),
    },
    depends: fake_program,
    suite: 'unit',
  )
endforeach
