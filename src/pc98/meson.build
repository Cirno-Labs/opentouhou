subdir('opna')

pc98_include = [layer_include['pc98']]
pc98_lib = static_library(
  'pc98',
  'mdrv.c',
  'mdrv_data.c',
  'pmd/driver.c',
  'pmd/chip.c',
  'pmd/sequence.c',
  'pmd/fm.c',
  'pmd/ssg.c',
  'pmd/drums.c',
  'pmd/rhythm.c',
  'pmd/part.c',
  'pmd_data.c',
  'pmd/effect.c',
  'pmd/registers.c',
  'pmd/lfo.c',
  'pmd/pitch.c',
  'pmd/random.c',
  'opna.c',
  'vsync.c',
  'zun/bfnt.c',
  'zun/logo1.c',
  'zun/master_rng.c',
  'zun/math16.c',
  'zun/pf.c',
  'zun/pi.c',
  'zun/sprite_sheet.c',
  c_args: pc98_include,
  implicit_include_directories: false,
  dependencies: [core_dep, opna_dep],
)
pc98_dep = declare_dependency(
  compile_args: pc98_include,
  link_with: pc98_lib,
  dependencies: [core_dep, opna_dep],
)
