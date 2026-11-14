# Everything but main.c, entry.c and stubs.c, so the tests link it too;
# stubs.c stays out so a test can link its own stand-ins for the unported
# modules instead.
th07_lib = static_library(
  'th07_lib',
  'files.c',
  'game.c',
  'scene.c',
  'settings.c',
  c_args: [layer_include['th07'], layer_include['shell']],
  implicit_include_directories: false,
  dependencies: [core_dep, stg_dep],
)

# th07 joins the release trains, with src/th07/version.txt, once it is
# packaged. Until then every build reports this development version.
th07_version = configure_file(
  output: 'version.h',
  configuration: {'GAME_VERSION': '"0.0.0-dev"'},
)

th07_main_lib = static_library(
  'th07_main',
  'main.c',
  c_args: [layer_include['th07'], layer_include['shell'], '-I' + meson.current_build_dir()],
  implicit_include_directories: false,
  dependencies: [core_dep, stg_dep],
)

# entry.c is the only source of the game that sees SDL's headers. It is a
# source of the executable, not of a library, because the entry point that
# Windows starts from (WinMain) is looked up only after the libraries are
# searched.
games += {
  'th07': {
    'sources': files('entry.c', 'stubs.c'),
    'c_args': [layer_include['th07']],
    'link_with': [th07_main_lib, th07_lib],
  },
}
