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

# Each part of core/base has a test named after it.
foreach part : ['log', 'memory', 'string']
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

# These tests do something fatal, which aborts the program, so they pass
# when they fail.
foreach name : ['allocator_failure', 'assert', 'memory_overflow']
  test(
    'core/base/' + name,
    executable(
      'test_' + name,
      'test_' + name + '.c',
      c_args: core_include,
      implicit_include_directories: false,
      link_with: core_base_lib,
    ),
    should_fail: true,
    suite: 'unit',
  )
endforeach
