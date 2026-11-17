opna_backend = static_library(
  'pc98_opna_backend',
  'vendor/emu/cores/fmopn.c',
  'vendor/emu/cores/ay8910.c',
  'vendor/emu/cores/ymdeltat.c',
  'vendor/emu/logging.c',
  c_args: ['-DHAVE_STDINT_H', '-DSNDDEV_SELECT', '-DSNDDEV_YM2608', '-w'],
  implicit_include_directories: false,
)
opna_dep = declare_dependency(
  link_with: opna_backend,
  dependencies: cc.find_library('m', required: false),
)
