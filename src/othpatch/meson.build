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
  'game.c',
  othpatch_fetch_backends[host_machine.system()],
  'language.c',
  'main.c',
  'release.c',
  'repo.c',
  'sha256.c',
  'update.c',
  'zip.c',
  othpatch_version,
  c_args: [layer_include['othpatch']] + core_include + ['-I' + meson.current_build_dir()],
  implicit_include_directories: false,
  dependencies: [curl, sdl3, zlib],
)

othpatch_libs = [othpatch_lib, core_json_lib, core_io_lib, core_base_lib]
othpatch_deps = [curl, sdl3, zlib]
othpatch_entry = files(
  host_machine.system() == 'windows' ? 'backend_main_windows.c' : 'backend_main_posix.c',
)
