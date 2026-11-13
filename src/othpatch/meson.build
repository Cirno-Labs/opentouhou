curl = dependency('libcurl', static: true)

othpatch_fetch_backends = {
  'linux': 'fetch/backend_linux.c',
  'darwin': 'fetch/backend_mac.c',
  'windows': 'fetch/backend_windows.c',
}

othpatch_install_backends = {
  'linux': 'install/backend_linux.c',
  'darwin': 'install/backend_mac.c',
  'windows': 'install/backend_windows.c',
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
    '--define',
    'RELEASES_URL=' + get_option('releases_url'),
  ],
  build_always_stale: true,
)

othpatch_lib = static_library(
  'othpatch',
  'fetch/curl.c',
  'fetch/file.c',
  'game.c',
  'install/install.c',
  othpatch_install_backends[host_machine.system()],
  othpatch_fetch_backends[host_machine.system()],
  'console.c',
  'context.c',
  'main.c',
  'release.c',
  'step_list.c',
  'step_patches.c',
  'step_remove.c',
  'step_upgrade.c',
  'zip.c',
  othpatch_version,
  c_args: [layer_include['othpatch']] + shell_include + ['-I' + meson.current_build_dir()],
  implicit_include_directories: false,
  dependencies: [curl, zlib],
)

othpatch_libs = [
  othpatch_lib, shell_lib, core_patch_select_lib, core_ini_lib,
  core_json_lib, core_io_lib, core_hash_lib, core_base_lib,
]
othpatch_deps = [curl, sdl3, zlib]
othpatch_entry = [
  files(host_machine.system() == 'windows' ? 'main_windows.c' : 'main_posix.c'),
  othpatch_version,
]
if host_machine.system() == 'windows'
  othpatch_entry += import('windows').compile_resources(
    'othpatch.rc',
    args: ['--include-dir', meson.current_build_dir(),
           '--include-dir', meson.project_source_root() / 'tools/packaging/othpatch'],
    depends: othpatch_version,
    depend_files: files('../../tools/packaging/othpatch/icon.ico'),
  )
endif
othpatch_version_include = '-I' + meson.current_build_dir()
