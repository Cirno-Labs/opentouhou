core_include = [layer_include['core']]

# Reads sources.txt into core_sources, a list of sources by library.
# sources.txt lists only the libraries that replay/retail's own project
# builds too; the others list their sources here.
core_sources = {}
core_library = ''
foreach line : import('fs').read('sources.txt').split('\n')
  if line.startswith('[')
    core_library = line.strip('[]')
    core_sources += {core_library: []}
  elif line != '' and not line.startswith('#')
    core_sources += {core_library: core_sources[core_library] + [line]}
  endif
endforeach

# core/math uses sqrtf, floorf, trunc and friends from the C library (never
# its trig or atan2, which core/math/libm vendors instead). GCC's libm is a
# separate archive on Linux; MinGW folds it into the C library, so there is
# nothing to find there.
cc_libm = meson.get_compiler('c').find_library('m', required: false)

# core/base needs nothing but the C library, so it is built without SDL and
# the tests link it alone.
core_base_lib = static_library(
  'core_base',
  core_sources['base'],
  c_args: core_include,
  implicit_include_directories: false,
)

# core/args needs only core/base, so every program and the tests link it
# without SDL.
core_args_lib = static_library(
  'core_args',
  core_sources['args'],
  c_args: core_include,
  implicit_include_directories: false,
)

# core/hash needs only core/base, so the tests link it without SDL.
core_hash_lib = static_library(
  'core_hash',
  core_sources['hash'],
  c_args: core_include,
  implicit_include_directories: false,
)

# core/json needs only core/base, so the tests link it without SDL.
core_json_lib = static_library(
  'core_json',
  'json/get.c',
  'json/index.c',
  'json/merge.c',
  'json/parse.c',
  c_args: core_include,
  implicit_include_directories: false,
)

core_io_lib = static_library(
  'core_io',
  core_sources['io'],
  host_machine.system() == 'windows' ? 'io/backend_windows.c' : 'io/backend_posix.c',
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
)

# core/ini needs core/base and core/io, which needs SDL for its files.
core_ini_lib = static_library(
  'core_ini',
  core_sources['ini'],
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
)

# core/patch's selection and manager need core/base, core/hash, core/io,
# core/ini and core/json, so that othpatch can link them without core/vfs.
core_patch_select_lib = static_library(
  'core_patch_select',
  'patch/fetch.c',
  'patch/language.c',
  'patch/manager.c',
  'patch/repo.c',
  'patch/select.c',
  'patch/update.c',
  c_args: core_include,
  implicit_include_directories: false,
  link_with: core_ini_lib,
  dependencies: [sdl3],
)

# core/archive needs core/base, core/hash, core/io, zlib and core/vfs's
# types, so that othpatch can link it without core/vfs.
core_archive_lib = static_library(
  'core_archive',
  core_sources['archive'],
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [zlib],
)

# othpatch links core/base, core/json, core/io, core/ini, core/archive and
# core/patch's selection only, so a build of othpatch alone needs none of the libraries
# below.
if not build_games
  subdir_done()
endif

# core/math needs only core/base, so the tests link it without SDL. The
# math/libm sources are a vendored double-precision libm (see
# math/libm/libm_internal.h), used only by math/x87.c.
core_math_lib = static_library(
  'core_math',
  'math/common.c',
  'math/libm/atan.c',
  'math/libm/atan2.c',
  'math/libm/cos.c',
  'math/libm/cos_kernel.c',
  'math/libm/rem_pio2.c',
  'math/libm/rem_pio2_large.c',
  'math/libm/sin.c',
  'math/libm/sin_kernel.c',
  'math/libm/tan.c',
  'math/libm/tan_kernel.c',
  'math/vec.c',
  'math/x87.c',
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [cc_libm],
)

# core/config needs core/base and core/ini (and so core/io), so the tests
# link it without SDL beyond what core/io needs.
core_config_lib = static_library(
  'core_config',
  'config.c',
  c_args: core_include,
  implicit_include_directories: false,
)

# core/input needs core/base, core/ini and core/config, plus one platform
# backend chosen by the 'platform' option; the tests link input.c and
# names.c against input_backend_fake.c instead of a real backend.
input_backend_sources = {
  'sdl': 'input/backend_sdl.c',
}
core_input_lib = static_library(
  'core_input',
  'input/input.c',
  'input/names.c',
  input_backend_sources[get_option('platform')],
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
)

core_vfs_lib = static_library(
  'core_vfs',
  core_sources['vfs'],
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
)

# core/text's font registry asks the system for installed fonts through
# GDI on Windows, CoreText on macOS and fontconfig elsewhere.
if host_machine.system() == 'windows'
  text_backend = 'text/backend_windows.c'
  text_system = meson.get_compiler('c').find_library('gdi32')
elif host_machine.system() == 'darwin'
  text_backend = 'text/backend_macos.c'
  text_system = dependency('appleframeworks', modules: ['CoreFoundation', 'CoreText'])
else
  text_backend = 'text/backend_fontconfig.c'
  text_system = dependency('', required: false)
endif

core_text_lib = static_library(
  'core_text',
  'text/font.c',
  'text/font_registry.c',
  'text/font_rom.c',
  'text/gdi.c',
  'text/glyphs.c',
  'text/render.c',
  text_backend,
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3_ttf, sdl3, freetype, text_system, cc_libm],
)

# core/patch needs core/base, core/io, core/vfs, core/json and core/text.
core_patch_lib = static_library(
  'core_patch',
  'patch/files.c',
  'patch/stack.c',
  'patch/strings.c',
  c_args: core_include,
  implicit_include_directories: false,
  link_with: [core_patch_select_lib, core_text_lib],
  dependencies: [sdl3],
)

# core/image needs core/base and core/io, plus libjpeg and FFmpeg;
# the tests link it without SDL beyond what core/io needs.
core_image_lib = static_library(
  'core_image',
  core_sources['image'],
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [avcodec, avutil, swscale, jpeg, cc_libm],
)

core_audio_lib = static_library(
  'core_audio',
  'audio/engine.c',
  'audio/decoder.c',
  'audio/resampler.c',
  host_machine.system() == 'emscripten' ? 'audio/backend_web.c' : 'audio/backend_sdl.c',
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3, avformat, avcodec, avutil, swresample, cc_libm],
)

# core/gfx's frontend batches, keeps the last two ticks and blends them
# without a GPU, so the tests link it alone. The OpenGL backend, with
# Gfx_Init, is a separate library.
core_gfx_lib = static_library(
  'core_gfx',
  'gfx/backend_null.c',
  'gfx/frontend.c',
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [cc_libm],
)

gfx_shaders = custom_target(
  'gfx_shaders',
  input: ['gfx/vertex.glsl', 'gfx/fragment.glsl'],
  output: 'gfx_shaders.h',
  command: [python3, files('../../tools/meson/embed_shader'), '@OUTPUT@', '@INPUT@'],
)

core_gfx_gl_lib = static_library(
  'core_gfx_gl',
  'gfx/backend_gl.c',
  gfx_shaders,
  c_args: core_include + ['-I' + meson.current_build_dir()],
  implicit_include_directories: false,
  dependencies: [sdl3, cc_libm],
)

# Layers above core never see the headers of its external libraries.
core_dep = declare_dependency(
  compile_args: core_include,
  link_with: [
    core_gfx_gl_lib, core_gfx_lib, core_input_lib, core_patch_lib,
    core_patch_select_lib, core_archive_lib, core_vfs_lib,
    core_config_lib, core_ini_lib, core_audio_lib, core_text_lib,
    core_image_lib, core_io_lib, core_json_lib, core_math_lib, core_args_lib,
    core_hash_lib, core_base_lib,
  ],
  dependencies: [text_system, cc_libm],
)
