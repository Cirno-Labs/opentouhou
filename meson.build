project(
  'opentouhou',
  'c',
  version: '0',
  license: 'GPL-3.0-only',
  meson_version: '>=1.4.0',
  default_options: [
    'c_std=c23',
    'warning_level=3',
    'b_ndebug=if-release',
  ],
)

cc = meson.get_compiler('c')
python3 = import('python').find_installation()

# The game logic must reproduce the original games' floating-point
# results, so the compiler may not fuse or reorder float operations, and
# x86 uses SSE2 instead of the x87 unit.
add_project_arguments('-ffp-contract=off', '-fno-fast-math', language: 'c')
if host_machine.cpu_family() in ['x86', 'x86_64']
  add_project_arguments('-msse2', '-mfpmath=sse', language: 'c')
endif

# The container macros return typed pointers, so a pointer of the wrong
# type is an error, as it is by default in GCC 14.
add_project_arguments(
  cc.get_supported_arguments(
    '-Werror=incompatible-pointer-types',
    '-Wno-unused-parameter',
    '-Wshadow',
    '-Wstrict-prototypes',
  ),
  language: 'c',
)

if get_option('buildtype') == 'release' and host_machine.system() != 'emscripten'
  add_project_link_arguments('-s', language: 'c')
endif

if host_machine.system() == 'windows'
  # Links libgcc and winpthread into the executable, so it needs no DLLs
  # beyond the ones Windows has.
  add_project_link_arguments('-static', language: 'c')
  # Selects MinGW's own printf family, which follows C99 (%zu, %lld) on
  # every Windows version, instead of the one in msvcrt.dll.
  add_project_arguments('-D__USE_MINGW_ANSI_STDIO=1', language: 'c')
endif

if host_machine.system() == 'emscripten'
  # The page downloads the wasm, writes the game's zip into the file
  # system and then calls main. The audio decoder is a thread, so the
  # page needs cross-origin isolation for shared memory.
  add_project_arguments('-pthread', language: 'c')
  add_project_link_arguments(
    '-pthread',
    '-sPTHREAD_POOL_SIZE=4',
    '-sINITIAL_MEMORY=64MB',
    '-sALLOW_MEMORY_GROWTH',
    '-sSTACK_SIZE=4MB',
    '-sDEFAULT_PTHREAD_STACK_SIZE=1MB',
    '-sMIN_WEBGL_VERSION=2',
    '-sMAX_WEBGL_VERSION=2',
    '-sGL_ENABLE_GET_PROC_ADDRESS',
    '-sINVOKE_RUN=0',
    '-sEXPORTED_RUNTIME_METHODS=FS,IDBFS,ENV,callMain',
    '-sINCOMING_MODULE_JS_API=canvas,print,printErr,onAbort,onRuntimeInitialized,wasmBinary',
    '-lidbfs.js',
    language: 'c',
  )
  # emcc runs wasm-opt and minifies the JS only with -O at link.
  if get_option('buildtype') == 'release'
    add_project_link_arguments('-O' + get_option('optimization'), language: 'c')
  endif
endif

# Each layer has its own include root in the build directory. The root
# holds one link, named after the layer, to the layer's source directory,
# so that a file includes "shell/game.h" and a layer cannot include a layer
# that is not on its include path.
include_root = meson.current_build_dir() / 'include'
layer_include = {}
foreach layer : ['core', 'shell', 'stg', 'pc98', 'th01', 'th06', 'th07', 'othpatch']
  run_command(
    python3,
    files('tools/meson/link_layer'),
    meson.current_source_dir() / 'src' / layer,
    include_root / layer / layer,
    check: true,
  )
  layer_include += {layer: '-I' + include_root / layer}
endforeach

programs = get_option('programs')
build_games = programs != 'othpatch'
build_othpatch = programs != 'games'

# The external libraries by variable name, each with its dependency and
# the name of its archive. A library or test sees only the headers of an
# external library. A program links the archives through external_deps,
# and the tests link them from one shared library (see tests/meson.build).
external_libs = {
  'sdl3': [dependency('sdl3', version: '>=3.2.0', static: true), 'SDL3'],
  # The case tools compress calc rows with zlib, and othpatch unpacks game
  # packages with it. No game layer depends on it.
  'zlib': [dependency('zlib', static: true), 'z'],
}
if build_games
  external_libs += {
    'sdl3_ttf': [dependency('sdl3-ttf', version: '>=3.2.2', static: true), 'SDL3_ttf'],
    'freetype': [dependency('freetype2', static: true), 'freetype'],
    'jpeg': [dependency('libjpeg', static: true), 'jpeg'],
    # FFmpeg supplies image codecs and audio decoding and conversion.
    # The Dockerfiles enable only the formats these modules support.
    'avcodec': [dependency('libavcodec', static: true), 'avcodec'],
    'avutil': [dependency('libavutil', static: true), 'avutil'],
    'swscale': [dependency('libswscale', static: true), 'swscale'],
    'avformat': [dependency('libavformat', static: true), 'avformat'],
    'swresample': [dependency('libswresample', static: true), 'swresample'],
  }
endif
if build_othpatch
  external_libs += {'curl': [dependency('libcurl', static: true), 'curl']}
endif
external_deps = []
foreach name, lib : external_libs
  external_deps += lib[0]
  set_variable(name, lib[0].partial_dependency(compile_args: true, includes: true))
endforeach

subdir('src/core')
subdir('src/shell')
if build_games
  subdir('src/stg')
  subdir('src/pc98')

  # Each game's directory adds the game to this dictionary. The executables
  # are declared here, so that they land in the root of the build directory.
  # An executable is named after the game id with an o before it, as
  # oth06, so that it does not clash with the original game's executable
  # in the same folder. A game's 'fonts' lists its built-in fonts, as
  # paths below src/shell/builtin, which its programs carry in a zip. A
  # game with a 'title', the original's, gets a web page o<game>.html.
  games = {}
  subdir('src/th01')
  subdir('src/th06')
  subdir('src/th07')

  builtin_libs = {}
  if host_machine.system() == 'emscripten'
    # The web pages inline the sprite sheet of their progress bar's runner.
    web_runner = run_command(
      python3, files('tools/web/data_url'), files('src/shell/web/reimu_run.png'),
      check: true,
    ).stdout().strip()
  endif
  foreach name, game : games
    builtin_names = get_option('font') == 'free' ? game['fonts'] : []
    builtin_inputs = []
    foreach builtin_name : builtin_names
      builtin_inputs += files('src/shell/builtin' / builtin_name)
    endforeach
    builtin_zip = custom_target(
      name + '_builtin_zip',
      input: builtin_inputs,
      output: name + '_builtin.zip',
      command: [python3, shell_pack_zip, '@OUTPUT@', shell_builtin_dir] + builtin_names,
    )
    builtin_libs += {
      name: static_library(
        name + '_builtin',
        custom_target(
          name + '_builtin_files',
          input: builtin_zip,
          output: name + '_builtin_files.c',
          command: [python3, shell_embed_binary, '@OUTPUT@', 'g_ShellBuiltinFiles', '@INPUT@'],
        ),
      ),
    }
    executable(
      'o' + name,
      game['sources'],
      c_args: game['c_args'],
      link_with: game.get('link_with', []) + [builtin_libs[name]],
      implicit_include_directories: false,
      dependencies: [shell_game_dep, stg_dep, sdl3] + external_deps,
      win_subsystem: get_option('buildtype') == 'release' ? 'windows' : 'console',
    )
    if host_machine.system() == 'emscripten' and 'title' in game
      configure_file(
        input: 'src/shell/web/play.html.in',
        output: 'o' + name + '.html',
        configuration: {
          'RUNNER': web_runner,
          'PROGRAM': 'o' + name,
          'GAME_ID': name,
          'TITLE': game['title'],
          'TITLE_EN': game['title_en'],
          'SUBTITLE_EN': game['subtitle_en'],
        },
      )
      # The page shows the game's icon, the largest image of its .ico.
      custom_target(
        'o' + name + '_png',
        input: 'tools/packaging' / name / 'icon.ico',
        output: 'o' + name + '.png',
        command: [python3, files('tools/web/ico_png'), '@INPUT@', '@OUTPUT@'],
        build_by_default: true,
      )
    endif
  endforeach

  # The web builds the games alone.
  if host_machine.system() != 'emscripten'
    subdir('replay')
    # Each game's native backend program runs the game under the harness.
    executable(
      'th01native',
      'replay/th01/native_port.c',
      c_args: [layer_include['th01']] + stg_include + shell_include + harness_include,
      implicit_include_directories: false,
      link_with: [
        th01_main_lib, th01_harness_lib, th01_lib, core_hash_lib, harness_native_lib,
        harness_lib, builtin_libs['th01'],
      ],
      dependencies: [core_dep, stg_dep, pc98_dep, sdl3] + external_deps,
      win_subsystem: 'console',
    )
    executable(
      'th06native',
      'replay/th06/native_port.c',
      c_args: [layer_include['th06']] + stg_include + shell_include + harness_include,
      implicit_include_directories: false,
      link_with: [
        th06_main_lib, th06_harness_lib, th06_lib, harness_native_lib, harness_lib,
        builtin_libs['th06'],
      ],
      dependencies: [core_dep, stg_dep, sdl3] + external_deps,
      win_subsystem: 'console',
    )
    executable(
      'th07native',
      'replay/th07/native_port.c',
      'src/th07/play_time.c',
      c_args: [layer_include['th07']] + stg_include + shell_include + harness_include,
      implicit_include_directories: false,
      link_with: [
        th07_main_lib, th07_harness_lib, th07_lib, harness_native_lib, harness_lib,
        builtin_libs['th07'],
      ],
      dependencies: [core_dep, stg_dep, sdl3] + external_deps,
      win_subsystem: 'console',
    )
    # Each game's dosbox backend is a program and a library, which run inside
    # the opentouhou-dosbox-x image (replay/dosbox/dosbox.h) with the
    # dosbox-x.conf beside them. dostest is the backend's own test game.
    if host_machine.system() == 'linux'
      configure_file(
        input: 'replay/dosbox/dosbox-x.conf',
        output: 'dosbox-x.conf',
        copy: true,
      )
      dosbox_deps = external_deps + [cc_libm]
      dosbox_export = files('replay/dosbox/export.map')
      executable(
        'dostestdosbox',
        'replay/dosbox/launch.c',
        'replay/dosbox/test/main.c',
        c_args: core_include + harness_include + harness_dosbox_include,
        implicit_include_directories: false,
        link_with: [harness_lib],
        dependencies: dosbox_deps,
      )
      shared_library(
        'dostestdosbox',
        'replay/dosbox/test/attach.c',
        'replay/dosbox/test/port.c',
        name_prefix: '',
        c_args: core_include + harness_include + harness_dosbox_include,
        implicit_include_directories: false,
        link_with: [harness_dosbox_lib, harness_lib],
        link_args: ['-Wl,--version-script=' + dosbox_export[0].full_path()],
        link_depends: dosbox_export,
        dependencies: dosbox_deps,
      )
      executable(
        'th01dosbox',
        'replay/dosbox/launch.c',
        'replay/th01/dosbox_main.c',
        c_args: core_include + harness_include + harness_dosbox_include,
        implicit_include_directories: false,
        link_with: [harness_lib],
        dependencies: dosbox_deps,
      )
      shared_library(
        'th01dosbox',
        'replay/th01/dosbox_port.c',
        'replay/th01/dosbox_state.c',
        'replay/th01/harness.c',
        'src/th01/frame.c',
        name_prefix: '',
        c_args: [layer_include['th01']] + core_include + harness_include + harness_dosbox_include,
        implicit_include_directories: false,
        link_with: [harness_dosbox_lib, harness_lib],
        link_args: ['-Wl,--version-script=' + dosbox_export[0].full_path()],
        link_depends: dosbox_export,
        dependencies: dosbox_deps,
      )
    endif
    # Each game's recorder plays a practice stage with the bot to make a
    # new replay file.
    executable(
      'th06record',
      'replay/th06/recorder.c',
      c_args: [layer_include['th06']] + stg_include + shell_include + bot_include,
      implicit_include_directories: false,
      link_with: [bot_lib, th06_lib],
      dependencies: [core_dep, stg_dep, sdl3] + external_deps,
      win_subsystem: 'console',
    )
  endif
endif

if build_othpatch
  # othpatch downloads patches. It is a program on core and shell alone, and
  # the only one that links curl.
  subdir('src/othpatch')
  executable(
    'othpatch',
    othpatch_entry,
    c_args: [layer_include['othpatch'], othpatch_version_include] + shell_include,
    implicit_include_directories: false,
    link_with: othpatch_libs,
    dependencies: othpatch_deps + external_deps,
    link_args: host_machine.system() == 'windows' ? ['-municode'] : [],
    win_subsystem: 'console',
  )
endif

# A cross build builds the tests only for Windows, where they run outside
# the build image.
if not meson.is_cross_build() or host_machine.system() == 'windows'
  subdir('tests')
endif
