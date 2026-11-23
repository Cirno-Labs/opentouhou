subdir('opna')

pc98_include = [layer_include['pc98']]
pc98_lib = static_library(
  'pc98',
  'mdrv.c',
  'mdrv_data.c',
  'opna/opna.c',
  'vsync.c',
  'zun/logo1.c',
  'zun/master_rng.c',
  'zun/math16.c',
  'zun/pi.c',
  c_args: pc98_include,
  implicit_include_directories: false,
  dependencies: [core_dep, opna_dep],
)
pc98_dep = declare_dependency(
  compile_args: pc98_include,
  link_with: pc98_lib,
  dependencies: [core_dep, opna_dep],
)
