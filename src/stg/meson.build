# stg sees core's headers and its own; a game includes stg only through
# "stg/..." headers, never stg's sources directly.
stg_include = [layer_include['core'], layer_include['stg']]

stg_chain_lib = static_library(
  'stg_chain',
  'chain.c',
  c_args: stg_include,
  implicit_include_directories: false,
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

stg_dep = declare_dependency(
  compile_args: [layer_include['stg']],
  link_with: [
    stg_chain_lib, stg_lzss_lib, stg_pbg3_lib, stg_pbg4_lib, stg_rng_lib,
  ],
  dependencies: [core_dep],
)
