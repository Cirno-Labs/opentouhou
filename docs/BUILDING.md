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
just build-linux            # all games and othpatch, debug
just build-linux release    # all games and othpatch, release
just build-win              # Windows executables, cross-compiled
just build-win release
```

The games build in one image and othpatch in another (see "Docker
images"). The first build pulls the images for the target from the
registry, or builds them if it cannot (this takes a while, it compiles
every dependency). Later builds reuse them, and ccache keeps rebuilds
fast.

The games land in `build/<platform>/<target>/`, one executable per game:
`build/linux/debug/o<game>`, `build/win/release/o<game>.exe`. An
executable's name is the game id with an `o` before it, so that it does
not clash with the original game's executable in the same folder.
othpatch lands in `build/<platform>/othpatch/<target>/`.

### macOS

Docker cannot produce macOS executables, so a Mac builds natively. It
needs the Xcode command line tools, Meson, Ninja and pkg-config
(`brew install meson ninja pkgconf`):

```sh
just deps-mac               # once: the libraries, for arm64 and x86-64
just build-mac release      # build/mac-arm64/... and build/mac-x86_64/...
just test-mac
```

`tools/mac/build_deps` builds the libraries in the sets of the Docker
images, at the versions of `tools/docker/pins` and with the options of
the Linux Dockerfiles, into `build/mac-deps/<arch>/<set>/`. The games
build against the base and media sets, othpatch against the base and
othpatch sets.

`just` with no arguments lists every recipe.

### Build options

othpatch updates the games and itself from the release index,
`releases.json` (see "Releasing"). The Meson option `releases_url`
names it. The default, `https://opentouhou.invalid/releases.json`, is a
placeholder that reaches nothing, so a build that is packaged or
published must set it. The Docker and macOS builds take it from the
environment variable `RELEASES_URL`. GitHub Actions sets it to the index
in the R2 bucket of the repository that runs the workflow,
`<R2_PUBLIC_URL>/releases.json`, so that a fork updates from its own
releases.

The Meson option `programs` selects what a build contains: `games`, the
games and the replay runner, `othpatch`, or `all`, the default. Each
comes with its tests. A build of `othpatch` needs only SDL3, zlib and
curl, and a build of `games` does not need curl.

## Packaging

```sh
just package-linux release
just package-win release
just package-mac release    # on a Mac
```

This writes one package per game to `build/packages/`:

- `o<game>-<version>-Linux.zip`: `o<game>.AppImage`, a single executable
  file with everything inside.
- `o<game>-<version>-Windows.zip`: `o<game>.exe` and the files it needs.
- `o<game>-<version>-macOS.zip`: `o<game>.app`, one universal bundle for
  Apple silicon and Intel Macs. It has an ad-hoc signature only, so
  macOS asks before its first start (Open from the context menu).

`just package-othpatch-linux release` and its `-win` and `-mac`
counterparts write othpatch's executable as its release names it:
`othpatch-<version>-Linux`, `othpatch-<version>-Windows.exe`, and
`othpatch-<version>-macOS`, one universal executable with an ad-hoc
signature. A development version holds a `-` instead of the `+`, as
`0.1.0-dev-abcdef0`, so that the name is safe in a URL.

Every package contains the game's executable, othpatch and a readme
with the credits. The othpatch is the version that
`src/<game>/othpatch.txt` names, downloaded as the release index gives
it and checked against the SHA-256 digest in the index. The index is
the one of the build option `releases_url`, or the one that the
environment variable `RELEASE_INDEX_URL` names. So packaging needs the
network, and it fails when the index has no such othpatch. None contains any of the original game's files. The
executable carries the game's icon and version: in the Windows file
properties, the AppImage's desktop entry and the macOS bundle's
Info.plist. `--help` prints the version too. The icons are rendered
from `tools/packaging/<game>.svg` by `tools/packaging/make_icons` and
committed, so the build needs no converter.

Each program, a game or othpatch, has its own version in
`src/<program>/version.txt`: the version of its current or next
release, such as `1.2.0`. A release build reports that version. Every
other build adds `-dev` and the commit, as `1.2.0-dev+abcdef0`, or
`-dev` alone when git cannot name the commit, as in a Docker build of a
git worktree. The environment variable `OPENTOUHOU_RELEASE=1` makes a
build a release build. `just version th06` prints the version.

## Releasing

Each program has its own changelog, `docs/<program>/CHANGELOG.md`, which
collects the changes for players under `## Unreleased` as they land.
core has no version: a change in core goes into the changelog of each
game it affects. A check (`tools/lint/check_versions`, run by prek)
makes sure that each changelog has a section for the version in
`version.txt`, or unreleased changes when that version has no release
yet, and that each `othpatch.txt` holds a version.

A release has two steps: a commit that sets the versions, and a manual
run of the "Publish the releases" workflow. To release th06 1.2.0:

```sh
tools/bump th06 1.2.0 --commit
git push origin HEAD
gh workflow run release.yml --ref develop
```

`tools/bump` writes the version into `version.txt` and names th06's
unreleased section after the version and the date. To release othpatch
and make every game ship it, each game in a new patch release:

```sh
tools/bump othpatch 0.2.0 --with-games --commit
```

The workflow publishes every program whose `version.txt` names a
version that the release index does not have, from the commit it runs
on. A version without a changelog section waits until `tools/bump`
writes it. It builds and publishes othpatch first, then the games, whose
packages take the othpatch that they ship from the index. For each
program, `tools/release_index` uploads the files to the R2 bucket and
then adds the release to the index, and the workflow pushes the tag
`<program>-vX.Y.Z`. A run without
unreleased versions does nothing. There are no GitHub releases.

The R2 bucket holds the downloads:

- `<program>/<version>/<file>`: the files of a release, such as
  `th06/0.1.1/oth06-0.1.1-Linux.zip`. A published version never
  changes.
- `snapshot/<file>`: the development snapshot.
- `releases.json`: the release index, written after the files. Its
  format is described in `tools/shared/release_index.py`. othpatch and
  packaging read it.

The workflows need these repository settings:

- Variables `R2_ACCOUNT_ID` and `R2_BUCKET`: the bucket.
- Variable `R2_PUBLIC_URL`: the public URL of the bucket, without a
  `/` at the end.
- Secrets `R2_ACCESS_KEY_ID` and `R2_SECRET_ACCESS_KEY`: a key that can
  read and write the bucket.

Every push to `develop` replaces the development snapshot instead, when
the repository variable `PRERELEASE_ENABLE` is `true`. It holds the
game packages with the othpatch that they ship, the othpatch of the
commit, and each program's unreleased changes.

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

`o<game> --help` lists the arguments and prints the folder mode and the
folders that the game uses.

"Beside the executable" means the program folder: the folder that holds
the `.AppImage` file on Linux, the folder that holds `o<game>.app` on
macOS, and the folder of the executable elsewhere. So on Linux and macOS,
put the AppImage or the bundle into the game's folder.

The data directory is never written to. Settings, scores and replays go
to the user folder, and thcrap patches come from the patches folder:

| Mode | User folder | Patches folder |
| --- | --- | --- |
| default | your user directory | `patches/` beside the user directories |
| portable | `userdata/` in the program folder | `patches/` in the program folder |
| `--user-dir DIR` | `DIR` | as in the default mode |
| `--user-dir DIR`, portable | `DIR` | `patches/` in the program folder |

A file `portable.txt` in the program folder turns on portable mode.
`--user-dir` moves only the user folder of one game, so that all games of
one installation share one patches folder. The user directories are:

| Platform | User directory |
| --- | --- |
| Linux | `$XDG_DATA_HOME/opentouhou/<game>/` (usually `~/.local/share/...`) |
| Windows | `%APPDATA%\opentouhou\<game>\` |
| macOS | `~/Library/Application Support/opentouhou/<game>/` |

Press F11 or Alt+Enter to toggle fullscreen. Press Print Screen or F12
to save a screenshot in `screenshots/` under the user folder. The
`[screenshot]` settings in `config.ini` select the native or window
resolution, PNG or JPEG, and JPEG quality. `[video] interpolation`
smooths motion between game ticks and is on by default; set it to
`false` to show each tick as the game drew it.

The games read thcrap patches from the patches folder
(`$XDG_DATA_HOME/opentouhou/patches/`, `%APPDATA%\opentouhou\patches\`
or `patches/` in portable mode). It has the layout of thcrap's
`repos/` folder, so a copy of that folder works: one folder per
repository, and in it one folder per patch. Every patch in it applies,
after the patches it depends on. `[patches] enabled = false` in
`config.ini` turns them off, and `[patches] disabled` lists patch ids,
such as `thpatch/lang_de`, separated by commas, that are left out.

`othpatch` fills the patches folder from the repositories of
thpatch.net. `othpatch` downloads the language pack of the system
language, `othpatch en` the English one; `othpatch --help` lists the
other arguments. A run also updates every patch in the folder.

Then `othpatch` checks for a newer release of each game beside it and
asks before it installs it; `othpatch --update` installs it without
asking. It then does the same for itself. The releases come from the
release index that the build option `releases_url` names. A development
build is never updated.

## Docker images

Each target platform has three images, defined under
`tools/docker/<platform>/<image>/`:

- `base`: the compiler, Meson, Ninja, ccache, and SDL3 and zlib, which
  every program links.
- `media`, built on `base`: SDL3_ttf with FreeType and HarfBuzz, and
  FFmpeg. The games, the replay runner and their tests build in it.
- `othpatch`, built on `base`: curl, and mbedTLS on Linux. othpatch and
  its tests build in it, so othpatch never links FFmpeg or SDL3_ttf.

On Linux the images are based on AlmaLinux 8, whose glibc 2.28 lets the
executables run on older distributions, with clang; the media image
also has appimagetool for the AppImages. On Windows they have MinGW-w64
for x86-64 and use the Meson cross file
`tools/docker/win/meson_cross_x86_64.txt`; the media image also has
MinGW-w64 for i686 with SDL3 and zlib, for the capture tools. The
dependencies are static libraries, and the Windows executables link the
MinGW runtime statically, so they need no DLLs beyond those of Windows.

`tools/docker/pins` holds the version and SHA-256 digest of every
library and tool that the images and `tools/mac/build_deps` download.
`tools/docker/image` gives each Dockerfile the pins that it declares as
ARGs. Each dependency is built in its own stage, and the final stage
collects its files into `/ext`.

`tools/docker/image` also names the images. A tag is the date of the
last commit that changed the image's Dockerfile or its parent's, and a
hash of the Dockerfile, its pins and the parent's tag. So a change to
the base image retags the media and othpatch images, and a change to one
pin retags only the images that use it. The images are pushed to
`ghcr.io/cirno-labs/`, so CI and most developers pull them instead of
building them. In GitHub Actions the prefix is the owner of the
repository instead, so a fork builds and pulls its own images. An owner
name that Docker does not accept in an image name gets local images
instead: nothing is pushed, and CI keeps each image in its own entry of
the Actions cache under its tag, so a job builds an image only after it
changes.

```sh
just image-linux     # build the three Linux images locally
just image-win
```

Change a Dockerfile or a pin, then rebuild and push the new images
before the change lands, or CI will build them from scratch on every
run. The "Build and push the Docker images" workflow pushes all images.

The images pin SDL3, SDL3_ttf and its FreeType and HarfBuzz libraries,
and FFmpeg for `core/image` and `core/audio`. FFmpeg supports PNG,
MJPEG and BMP images, and WAV, Ogg Vorbis, FLAC and MP3 audio. No GPL
component is enabled, so its licence stays LGPL.

They also pin curl for othpatch, cut down to HTTP and HTTPS. Its TLS is
mbedTLS on Linux and macOS and Schannel on Windows. curl has no CA bundle
of its own; othpatch uses the system's certificates. Only othpatch links
curl and mbedTLS.

## Building without Docker

Possible, not supported. You need a C23 compiler (clang 18 or GCC 14 or
newer), Meson 1.4 or newer, Ninja, pkg-config, SDL3 3.2 or newer, SDL3_ttf 3.2.2 or newer, and
FFmpeg 7.1 development libraries (avcodec, avformat, avutil, swresample
and swscale). Then:

```sh
meson setup build/native
meson compile -C build/native
```

`tools/docker/pins` and the Dockerfiles are the reference for which
versions and build options we use. Builds that differ from them may behave differently, and bugs
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
just test           # unit tests, inside the Linux images
just test othpatch  # only othpatch's tests (or games)
```

Each image runs the tests of its programs. The tests of core/base,
core/json and core/io run in both.

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

- **Web:** an Emscripten image producing a WASM build.
