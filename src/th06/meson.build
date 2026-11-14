# Everything but main.c and entry.c, so the replay runner and the tests link it too.
# The shell/game functions that it calls come from shell/game in the game,
# from replay/th06/runner/shell_null.c in the runner and from
# tests/shell_fake.c in the tests.
th06_lib = static_library(
  'th06_lib',
  'angle.c',
  'anm/state.c',
  'anm/draw.c',
  'anm/texture.c',
  'anm/text.c',
  'ascii/chain.c',
  'ascii/pause_menu.c',
  'ascii/text.c',
  'bomb/update.c',
  'bomb/draw.c',
  'bullet.c',
  'buttons.c',
  'ecl.c',
  'effect.c',
  'enemy.c',
  'files.c',
  'game.c',
  'game_manager/chain.c',
  'game_manager/state.c',
  'game_manager/camera.c',
  'gui/gui.c',
  'gui/draw.c',
  'gui/text_image.c',
  'item.c',
  'note.c',
  'player.c',
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
  'sound/events.c',
  'settings.c',
  'sound/player.c',
  'stage.c',
  'strings.c',
  'timer.c',
  'translation.c',
  c_args: [layer_include['th06'], layer_include['shell']],
  implicit_include_directories: false,
  dependencies: [core_dep, stg_dep],
)

# Rewritten on every build; the compile reruns only when the version
# changed.
th06_version = custom_target(
  'th06_version',
  output: 'version.h',
  command: [python3, files('../../tools/meson/version_header'), 'th06', '@OUTPUT@'],
  build_always_stale: true,
)

th06_main_lib = static_library(
  'th06_main',
  'main.c',
  th06_version,
  c_args: [layer_include['th06'], layer_include['shell'], '-I' + meson.current_build_dir()],
  implicit_include_directories: false,
  dependencies: [core_dep, stg_dep],
)

# entry.c is the only source of the game that sees SDL's headers. It is a
# source of the executable, not of a library, because the entry point that
# Windows starts from (WinMain) is looked up only after the libraries are
# searched.
th06_sources = [files('entry.c')]
if host_machine.system() == 'windows'
  th06_sources += import('windows').compile_resources(
    'th06.rc',
    args: ['--include-dir', meson.current_build_dir(),
           '--include-dir', meson.project_source_root() / 'tools/packaging/th06'],
    depends: th06_version,
    depend_files: files('../../tools/packaging/th06/icon.ico'),
  )
endif

games += {
  'th06': {
    'sources': th06_sources,
    'c_args': [layer_include['th06']],
    'link_with': [th06_main_lib, th06_lib],
  },
}
