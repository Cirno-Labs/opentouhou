# Everything but main.c, entry.c and stubs.c, so the tests link it too;
# stubs.c stays out so a test can link its own stand-ins for the unported
# modules instead.
th07_lib = static_library(
  'th07_lib',
  'anm/draw.c',
  'anm/state.c',
  'anm/text.c',
  'anm/texture.c',
  'ascii/chain.c',
  'ascii/pause_menu.c',
  'ascii/text.c',
  'bomb/draw.c',
  'bomb/update.c',
  'bullet.c',
  'buttons.c',
  'ecl/file.c',
  'ecl/patterns.c',
  'ecl/run.c',
  'effect.c',
  'enemy.c',
  'files.c',
  'game.c',
  'game_manager/chain.c',
  'game_manager/cherry.c',
  'game_manager/state.c',
  'gui/draw.c',
  'gui/gui.c',
  'gui/msg.c',
  'item.c',
  'player/chain.c',
  'player/shot_type.c',
  'player/shots.c',
  'replay/chain.c',
  'replay/format.c',
  'replay/store.c',
  'scene.c',
  'screen_effect.c',
  'screens/ending/screen.c',
  'screens/ending/script.c',
  'screens/main.c',
  'screens/music/comments.c',
  'screens/music/screen.c',
  'screens/result/records.c',
  'screens/result/saves.c',
  'screens/result/score.c',
  'screens/result/screen.c',
  'settings.c',
  'sound/bgm.c',
  'sound/events.c',
  'sound/player.c',
  'stage.c',
  'strings.c',
  c_args: [layer_include['th07'], layer_include['shell']],
  implicit_include_directories: false,
  dependencies: [core_dep, stg_dep],
)

# Rewritten on every build; the compile reruns only when the version
# changed.
th07_version = custom_target(
  'th07_version',
  output: 'version.h',
  command: [python3, files('../../tools/meson/version_header'), 'th07', '@OUTPUT@'],
  build_always_stale: true,
)

th07_main_lib = static_library(
  'th07_main',
  'main.c',
  th07_version,
  c_args: [layer_include['th07'], layer_include['shell'], '-I' + meson.current_build_dir()],
  implicit_include_directories: false,
  dependencies: [core_dep, stg_dep],
)

# entry.c is the only source of the game that sees SDL's headers. It is a
# source of the executable, not of a library, because the entry point that
# Windows starts from (WinMain) is looked up only after the libraries are
# searched.
th07_sources = [files('entry.c', 'play_time.c')]
if host_machine.system() == 'windows'
  th07_sources += import('windows').compile_resources(
    'th07.rc',
    args: ['--include-dir', meson.current_build_dir(),
           '--include-dir', meson.project_source_root() / 'tools/packaging/th07'],
    depends: th07_version,
    depend_files: files('../../tools/packaging/th07/icon.ico'),
  )
endif

games += {
  'th07': {
    'sources': th07_sources,
    'c_args': [layer_include['th07']],
    'link_with': [th07_main_lib, th07_lib],
  },
}
