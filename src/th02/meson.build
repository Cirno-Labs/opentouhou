th02_lib = static_library(
  'th02_lib',
  'audio.c',
  'clip.c',
  'gaiji.c',
  'vsync_irq.c',
  'patterns.c',
  'random.c',
  'video.c',
  c_args: [layer_include['th02']],
  implicit_include_directories: false,
  dependencies: [core_dep, pc98_dep],
)
