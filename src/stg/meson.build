# stg sees core's headers and its own; a game includes stg only through
# "stg/..." headers, never stg's sources directly.
stg_include = [layer_include['core'], layer_include['stg']]

stg_angle_lib = static_library(
  'stg_angle',
  'angle.c',
  c_args: stg_include,
  implicit_include_directories: false,
  link_with: [core_math_lib],
)

stg_chain_lib = static_library(
  'stg_chain',
  'chain.c',
  c_args: stg_include,
  implicit_include_directories: false,
)

stg_end_diff_lib = static_library(
  'stg_end_diff',
  'end_diff.c',
  c_args: stg_include,
  implicit_include_directories: false,
  dependencies: [core_dep],
)

stg_lzss_lib = static_library(
  'stg_lzss',
  'lzss.c',
  c_args: stg_include,
  implicit_include_directories: false,
)

stg_pbg3_lib = static_library(
  'stg_pbg3',
  'pbg3.c',
  c_args: stg_include,
  implicit_include_directories: false,
  link_with: [core_archive_lib, stg_lzss_lib],
)

stg_pbg4_lib = static_library(
  'stg_pbg4',
  'pbg4.c',
  c_args: stg_include,
  implicit_include_directories: false,
  link_with: [core_archive_lib, stg_lzss_lib],
)

stg_rng_lib = static_library(
  'stg_rng',
  'rng.c',
  c_args: stg_include,
  implicit_include_directories: false,
)

stg_timer_lib = static_library(
  'stg_timer',
  'timer.c',
  c_args: stg_include,
  implicit_include_directories: false,
)

stg_translation_lib = static_library(
  'stg_translation',
  'translation.c',
  c_args: stg_include,
  implicit_include_directories: false,
  dependencies: [core_dep],
)

stg_note_lib = static_library(
  'stg_note',
  'note.c',
  c_args: stg_include,
  implicit_include_directories: false,
  dependencies: [core_dep],
  link_with: [stg_translation_lib],
)

stg_dep = declare_dependency(
  compile_args: [layer_include['stg']],
  link_with: [
    stg_angle_lib, stg_chain_lib, stg_end_diff_lib, stg_lzss_lib, stg_pbg3_lib, stg_pbg4_lib,
    stg_rng_lib, stg_timer_lib, stg_translation_lib, stg_note_lib,
  ],
  dependencies: [core_dep],
)
