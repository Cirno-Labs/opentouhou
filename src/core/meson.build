core_include = [layer_include['core']]

# core/math uses sqrtf, floorf, trunc and friends from the C library (never
# its trig or atan2, which core/math/libm vendors instead). GCC's libm is a
# separate archive on Linux; MinGW folds it into the C library, so there is
# nothing to find there.
cc_libm = meson.get_compiler('c').find_library('m', required: false)

# core/base needs nothing but the C library, so it is built without SDL and
# the tests link it alone.
core_base_lib = static_library(
  'core_base',
  'base/arena.c',
  'base/array.c',
  'base/cp932.c',
  'base/cp932_table.c',
  'base/hash_map.c',
  'base/log.c',
  'base/memory.c',
  'base/result.c',
  'base/sha1.c',
  'base/string.c',
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

core_io_lib = static_library(
  'core_io',
  'io/file.c',
  'io/path.c',
  'io/reader.c',
  'io/writer.c',
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
)

# core/ini needs core/base and core/io, which needs SDL for its files.
core_ini_lib = static_library(
  'core_ini',
  'ini/edit.c',
  'ini/ini.c',
  'ini/parse.c',
  'ini/write.c',
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
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
  'vfs/vfs.c',
  'vfs/dir_source.c',
  'vfs/normalize.c',
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
)

# core/image needs core/base and core/io, plus FFmpeg for decoding and
# encoding; the tests link it without SDL beyond what core/io needs.
core_image_lib = static_library(
  'core_image',
  'image/decode.c',
  'image/encode.c',
  'image/pixels.c',
  'image/resize.c',
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [avcodec, avutil, swscale, cc_libm],
)

core_audio_lib = static_library(
  'core_audio',
  'audio/engine.c',
  'audio/decoder.c',
  'audio/backend_sdl.c',
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3, avformat, avcodec, avutil, swresample, cc_libm],
)

core_lib = static_library(
  'core',
  'app/app.c',
  'app/clock.c',
  'app/hotkeys.c',
  'app/settings.c',
  c_args: core_include,
  implicit_include_directories: false,
  dependencies: [sdl3],
)

core_main_lib = static_library(
  'core_main',
  'app/main.c',
  implicit_include_directories: false,
  dependencies: [sdl3],
)

# SDL is a dependency of the library, not of its users: layers above core
# link it, but never see its headers.
core_dep = declare_dependency(
  compile_args: core_include,
  link_with: [
    core_lib, core_input_lib, core_vfs_lib, core_config_lib, core_ini_lib,
    core_audio_lib, core_image_lib, core_io_lib, core_json_lib, core_math_lib, core_base_lib,
  ],
  link_whole: core_main_lib,
  dependencies: [
    sdl3.partial_dependency(links: true),
    avcodec.partial_dependency(links: true),
    avutil.partial_dependency(links: true),
    swscale.partial_dependency(links: true),
    avformat.partial_dependency(links: true),
    swresample.partial_dependency(links: true),
    cc_libm,
  ],
)
