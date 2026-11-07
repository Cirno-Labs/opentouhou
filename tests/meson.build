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
