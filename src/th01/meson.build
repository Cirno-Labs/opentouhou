th01_lib = static_library(
  'th01_lib',
  'bfnt.c',
  'logo.c',
  'math.c',
  'input.c',
  'settings.c',
  'video.c',
  'pf.c',
  'pi.c',
  'ptn.c',
  'grc.c',
  'grf.c',
  'grz.c',
  c_args: [layer_include['th01']],
  implicit_include_directories: false,
  dependencies: [core_dep, stg_dep],
)

th01_version = custom_target(
  'th01_version',
  output: 'version.h',
  command: [python3, files('../../tools/meson/version_header'), 'th01', '@OUTPUT@'],
  build_always_stale: true,
)

th01_main_lib = static_library(
  'th01_main',
  'main.c',
  th01_version,
  c_args: [layer_include['th01'], layer_include['shell'], '-I' + meson.current_build_dir()],
  implicit_include_directories: false,
  dependencies: [core_dep, stg_dep],
)

th01_sources = [files('entry.c')]
if host_machine.system() == 'windows'
  th01_sources += import('windows').compile_resources(
    'th01.rc',
    args: ['--include-dir', meson.current_build_dir(),
           '--include-dir', meson.project_source_root() / 'tools/packaging/th01'],
    depends: th01_version,
    depend_files: files('../../tools/packaging/th01/icon.ico'),
  )
endif

games += {
  'th01': {
    'sources': th01_sources,
    'c_args': [layer_include['th01']],
    'fonts': ['fonts/ipag.ttf', 'fonts/ipagp.ttf'],
    'link_with': [th01_main_lib, th01_lib],
  },
}
