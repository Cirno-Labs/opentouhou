# Everything but main.c, so the replay runner and the tests link it too.
th06_lib = static_library(
  'th06_lib',
  'chain.c',
  'files.c',
  c_args: [layer_include['th06']],
  implicit_include_directories: false,
  dependencies: [core_dep, stg_dep],
)

games += {
  'th06': {
    'sources': files('main.c'),
    'c_args': [layer_include['th06']],
    'link_with': [th06_lib],
  },
}
