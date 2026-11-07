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

That is all. The compilers, Meson, SDL3, SDL3_ttf, FFmpeg and the rest
live in the images.

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
`build/linux/debug/oth06`, `build/win/release/oth06.exe`. An
executable's name is the game id with an `o` before it, so that it does
not clash with the original game's executable in the same folder.

`just` with no arguments lists every recipe.

## Packaging

```sh
just package-linux release
just package-win release
```

This writes one package per game to `build/packages/`:

- `oth06-<version>-Linux.AppImage`: a single executable file with
  everything inside.
- `oth06-<version>-Windows.zip`: `oth06.exe` and the files it needs.

Every package contains the game's executable, its shaders, the bundled
font and its licence (`BIZUDGothic`, SIL Open Font License), this
project's licence and a readme. None contains any of the original
game's files.

The version comes from `git describe` on the latest `v*` tag; untagged
builds are named after the commit.

## Running

The games need the original game's data. Point a game at the directory
that holds it, either by putting the executable there or by passing it:

```sh
build/linux/debug/oth06 --data ~/games/th06
```

The data directory is never written to unless you choose the original
save format (see ARCHITECTURE.md, "Data, saves and config"). Settings,
scores and replays go to your user directory:

| Platform | User directory |
| --- | --- |
| Linux | `$XDG_DATA_HOME/opentouhou/th06/` (usually `~/.local/share/...`) |
| Windows | `%APPDATA%\opentouhou\th06\` |

## Docker images

There is one image per target platform, defined under `tools/docker/`:

- `linux`: an old-glibc base, so the executables run on older
  distributions; clang; the dependencies built as static libraries.
- `win`: MinGW-w64 for x86-64, a Meson cross file, the dependencies
  cross-compiled as static libraries.

Each dependency is built in its own stage and installed into `/ext`,
which the final stage collects. The images are tagged with a version in
the `justfile` and pushed to a registry, so CI and most developers pull
them instead of building them.

```sh
just image-linux     # build the Linux image locally
just image-win
```

Change a Dockerfile, bump the image version in the `justfile`, rebuild,
and push the new image before the change lands, or CI will build it from
scratch on every run.

FFmpeg is built with only what the games use: the PNG, JPEG and BMP
image decoders, the PCM, Vorbis, FLAC and MP3 audio decoders, their
demuxers, and libswresample. Its flags live in one file next to the
Dockerfiles.

## Building without Docker

Possible, not supported. You need a C23 compiler (clang 18 or GCC 14 or
newer), Meson 1.3 or newer, Ninja, pkg-config, and development packages
for SDL3, SDL3_ttf and FFmpeg. Then:

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
just test-replays   # replay tests, needs your own game data and cases
```

Unit tests need nothing but the source and run in CI. Replay tests need
the original game data and replay cases produced from the original
executable, so they run only on your machine; see ARCHITECTURE.md,
"Replay tests".

## Later

- **macOS:** a native build with Apple's clang outside Docker, as Docker
  cannot produce macOS executables.
- **Web:** an Emscripten image producing a WASM build.
