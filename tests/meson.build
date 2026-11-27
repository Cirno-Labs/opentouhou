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

# A test's TEST_DIR or XDG_DATA_HOME must stay under tests/tmp/ (see
# test_dir.h), which this gives areas in their own subdirectory in place of
# meson.current_build_dir() / 'tmp'.
tests_tmp = meson.current_build_dir() / 'tmp'

# Tests that do something fatal pass only when the program aborts with a
# fatal error that contains the given text. core, tools and pc98 all have
# fatal-error tests, so the checker and the Python it runs under are shared.
python = find_program('python3.12', 'python3')
expect_fatal = files('expect_fatal.py')

subdir('core')
subdir('tools')

if build_othpatch
  subdir('othpatch')
endif

# The other tests need the libraries of the games.
if not build_games
  subdir_done()
endif

subdir('shell')
subdir('stg')
subdir('replay')
subdir('th01')
subdir('pc98')
subdir('th02')
subdir('th06')
subdir('th07')
