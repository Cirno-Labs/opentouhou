# Everything but main.c and stubs.c, so the replay runner and the tests
# link it too; stubs.c stays out so a test can link its own stand-ins for
# the unported modules instead.
th06_lib = static_library(
  'th06_lib',
  'angle.c',
  'anm/state.c',
  'bomb/update.c',
  'bullet.c',
  'buttons.c',
  'chain.c',
  'cycle_stubs.c',
  'ecl.c',
  'effect.c',
  'files.c',
  'game.c',
  'game_manager/chain.c',
  'item.c',
  'player.c',
  'scene.c',
  'screen_effect.c',
  'stage.c',
  'timer.c',
  c_args: [layer_include['th06']],
  implicit_include_directories: false,
  dependencies: [core_dep, stg_dep],
)

games += {
  'th06': {
    'sources': files('main.c', 'stubs.c'),
    'c_args': [layer_include['th06']],
    'link_with': [th06_lib],
  },
}
