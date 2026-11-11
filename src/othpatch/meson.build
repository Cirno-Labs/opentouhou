curl = dependency('libcurl', static: true)

othpatch_fetch_backends = {
  'linux': 'fetch/backend_linux.c',
  'darwin': 'fetch/backend_mac.c',
  'windows': 'fetch/backend_windows.c',
}

# Rewritten on every build; the compile reruns only when the version
# changed.
othpatch_version = custom_target(
  'othpatch_version',
  output: 'version.h',
  command: [
    python3,
    files('../../tools/meson/version_header'),
    'othpatch',
    '@OUTPUT@',
    '--prefix',
    'OTHPATCH',
  ],
  build_always_stale: true,
)

othpatch_lib = static_library(
  'othpatch',
  'fetch/curl.c',
  'fetch/file.c',
  othpatch_fetch_backends[host_machine.system()],
  'language.c',
  'repo.c',
  'update.c',
  othpatch_version,
  c_args: [layer_include['othpatch']] + core_include + ['-I' + meson.current_build_dir()],
  implicit_include_directories: false,
  dependencies: [curl, sdl3],
)

othpatch_libs = [othpatch_lib, core_json_lib, core_io_lib, core_base_lib]
othpatch_deps = [curl, sdl3]
