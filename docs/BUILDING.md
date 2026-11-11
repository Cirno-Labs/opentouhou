# Building

The supported way to build is inside Docker, driven by
[just](https://github.com/casey/just). The images contain the compilers
and every dependency, built statically, so the result does not depend on
what your system has installed, and a Linux machine can build the
Windows executables without installing MinGW. The release builds come
out of the same images.

## Requirements

- Docker (or Podman with its Docker-compatible CLI)
- just
- git
- Python 3, for the tools under `tools/`

That is all. The compilers, Meson and the libraries live in the images.

## Building

```sh
just build-linux            # all games, debug
just build-linux release    # all games, release
just build-win              # Windows executables, cross-compiled
just build-win release
```

The first build pulls the image for the target from the registry, or
builds it if it cannot (this takes a while, it compiles every
dependency). Later builds reuse it, and ccache keeps rebuilds fast.

Outputs land in `build/<platform>/<target>/`, one executable per game:
`build/linux/debug/o<game>`, `build/win/release/o<game>.exe`. An
executable's name is the game id with an `o` before it, so that it does
not clash with the original game's executable in the same folder.

`just` with no arguments lists every recipe.

## Packaging

```sh
just package-linux release
just package-win release
```

This writes one package per game to `build/packages/`:

- `o<game>-<version>-Linux.AppImage`: a single executable file with
  everything inside.
- `o<game>-<version>-Windows.zip`: `o<game>.exe` and the files it needs.

Every package contains the game's executable, this project's licence
and a readme. None contains any of the original game's files.

The version comes from `git describe` on the latest `v*` tag; untagged
builds are named after the commit.

## Capturing replay test cases

```sh
just replay::build <game>
```

This cross-builds a game's capture tools, dev-only tools for capturing
replay test cases from the original game. They are used locally and
never packaged or released. Each game's own notes describe how to
capture with them.

## Comparing replay test cases

`th06replay` compares captured calc rows with headless playback. It is
built with the games and is never packaged. Pass the original data, the
player's replay input and its captured case:

```sh
build/linux/debug/th06replay --data <data-dir> --replay <input.rpy> <case.otrc>
just replay::run <case-or-dir> <data-dir> <replay-dir>
just replay::run-fast <case-or-dir> <data-dir> <replay-dir> [jobs]
```

`run-fast` uses a release build and runs one process per case, as many
at once as there are CPUs unless `jobs` says otherwise. It prints each
case's result and the output of the failed ones.

`--replays <dir>` matches each case to a `.rpy` with the same basename.
`--break-on-diff` stops in the debugger at the first differing calc.
`--break-at N` stops at calc N; calc numbers start at 0. Cases and replay
inputs stay outside the repository. Original files are only read.

The runner reports skipped columns and the first calc with any
difference. Its [public header](../replay/th06/runner/run.h) describes
the supported capture format and the checks for full replay coverage.

## Running

The games need the original game's data. Point a game at the directory
that holds it, either by putting the executable there or by passing it:

```sh
build/linux/debug/o<game> --data ~/games/<game>
```

The data directory is never written to unless you choose the original
save format (see ARCHITECTURE.md, "Data, saves and config"). Settings,
scores and replays go to your user directory:

| Platform | User directory |
| --- | --- |
| Linux | `$XDG_DATA_HOME/opentouhou/<game>/` (usually `~/.local/share/...`) |
| Windows | `%APPDATA%\opentouhou\<game>\` |

Press F11 or Alt+Enter to toggle fullscreen. Press Print Screen or F12
to save a screenshot in `screenshots/` under the user directory. The
`[screenshot]` settings in `config.ini` select the native or window
resolution, PNG or JPEG, and JPEG quality. `[video] interpolation`
smooths motion between game ticks and is on by default; set it to
`false` to show each tick as the game drew it.

## Docker images

There is one image per target platform, defined under `tools/docker/`:

- `linux`: AlmaLinux 8, whose glibc 2.28 lets the executables run on
  older distributions; clang; the dependencies built as static
  libraries; appimagetool for the AppImages.
- `win`: MinGW-w64 for x86-64, a Meson cross file, the dependencies
  cross-compiled as static libraries. The executables link the MinGW
  runtime statically, so they need no DLLs beyond those of Windows.

Each dependency is built in its own stage and installed into `/ext`,
which the final stage collects. Each image is tagged with the
committer date and a hash of its own Dockerfile, computed by
`tools/docker/image_tag`, and pushed to `ghcr.io/cirno-labs/`, so CI and
most developers pull them instead of building them. Changing one
Dockerfile only retags that platform's image.

```sh
just image-linux     # build the Linux image locally
just image-win
```

Change a Dockerfile, then rebuild and push the new image before the
change lands, or CI will build it from scratch on every run. The "Build
and push the Docker images" workflow pushes both images.

The images pin SDL3, SDL3_ttf and its FreeType and HarfBuzz libraries,
and FFmpeg for `core/image` and `core/audio`. FFmpeg supports PNG,
MJPEG and BMP images, and WAV, Ogg Vorbis, FLAC and MP3 audio. No GPL
component is enabled, so its licence stays LGPL.

## Building without Docker

Possible, not supported. You need a C23 compiler (clang 18 or GCC 14 or
newer), Meson 1.4 or newer, Ninja, pkg-config, SDL3 3.2 or newer, SDL3_ttf 3.2.2 or newer, and
FFmpeg 7.1 development libraries (avcodec, avformat, avutil, swresample
and swscale). Then:

```sh
meson setup build/native
meson compile -C build/native
```

The Dockerfiles are the reference for which versions and build options
we use. Builds that differ from them may behave differently, and bugs
that only appear there may not be fixed.

## Code style and checks

Formatting is clang-format, with the configuration in `.clang-format`.
The checks run through [prek](https://github.com/j178/prek), configured
in `.pre-commit-config.yaml`:

```sh
prek install    # once: runs the checks on every commit and commit message
prek -a         # runs them on the whole tree
```

The style and commit rules the checks enforce are in
[AGENTS.md](../AGENTS.md).

## Tests

```sh
just test           # unit tests, inside the Linux image
```

Unit tests live in `tests/`. They need nothing but the source and run
in CI. Replay comparisons use private cases and original data, so they
run separately from `just test`.

The graphics pixel tests also need an OpenGL driver. Run the built test
outside Docker, with SDL's offscreen video driver:

```sh
SDL_VIDEODRIVER=offscreen build/linux/debug/tests/test_gfx_gl
SDL_VIDEODRIVER=offscreen TEST_GFX_ES=1 build/linux/debug/tests/test_gfx_gl
```

The second command checks the same renderer through OpenGL ES 3.0.
These tests use generated images and do not need game data.

## Later

- **macOS:** a native build with Apple's clang outside Docker, as Docker
  cannot produce macOS executables.
- **Web:** an Emscripten image producing a WASM build.
