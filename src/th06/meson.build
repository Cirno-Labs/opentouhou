# Everything but main.c, so the replay runner and the tests link it too.
# The core/app functions that it calls
# come from core/app in the game, from replay/th06/runner/app_null.c in
# the runner and from tests/app_fake.c in the tests.
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
  'chain.c',
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
  c_args: [layer_include['th06']],
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
th06_sources = [files('main.c'), th06_version]
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
    'c_args': [layer_include['th06'], '-I' + meson.current_build_dir()],
    'link_with': [th06_lib],
  },
}
