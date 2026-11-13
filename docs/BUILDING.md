# Building

This guide is for developers: it describes how to build, test, check,
package and release the programs.

The supported way to build is inside Docker, driven by
[just](https://github.com/casey/just). The images contain the compilers
and every dependency, built statically, so the result does not depend on
what your system has installed. A Linux machine also builds the Windows
executables. The release builds come out of the same images.

## Requirements

- Docker, or Podman with its Docker-compatible CLI
- just
- git
- Python 3.11 or newer, for the tools under `tools/`
- [prek](https://github.com/j178/prek), for the checks

The compilers, Meson and the libraries are in the images.

## Quick start

```sh
just build-linux                  # every game and othpatch, debug
just test                         # the unit tests
just run <game> --data <folder>   # builds the games, starts one
```

`just` with no arguments lists every recipe. The build and package
recipes take `debug`, the default, or `release`, as in
`just build-win release`, which cross-compiles the Windows executables.

The first build pulls the images of the target from the registry, or
builds them if it cannot. This takes a while, because it compiles every
dependency. Later builds reuse the images, and ccache keeps rebuilds
fast.

The games land in `build/<platform>/<target>/`, one executable per game:
`build/linux/debug/o<game>`, `build/win/release/o<game>.exe`. The name
is the game id with an `o` before it, so that it does not clash with the
original game's executable in the same folder. othpatch lands in
`build/<platform>/othpatch/<target>/`.

A game needs the original game's data: pass its folder with `--data`,
or put the executable into that folder. The game's
`tools/packaging/<game>/README.txt`, which the packages ship, describes
the folders, the keys and the settings for players.

### Build options

- `programs` selects what a build contains: `games`, the games and the
  replay runner, `othpatch`, or `all`, the default. Each comes with its
  tests. A build of `othpatch` needs only SDL3, zlib and curl, and a
  build of `games` does not need curl.
- `werror` makes every compiler warning fail the build. CI sets it. The
  environment variable `OPENTOUHOU_WERROR=1` sets it for the Docker and
  macOS builds.
- `releases_url` names the release index, `releases.json`, from which
  othpatch updates the games and itself. The default,
  `https://opentouhou.invalid/releases.json`, reaches nothing, so a
  build that is packaged or published must set it. The Docker and macOS
  builds take it from the environment variable `RELEASES_URL`. GitHub
  Actions sets that variable to `<R2_PUBLIC_URL>/releases.json`, the
  index in the bucket of the repository that runs the workflow, when the
  repository variable `R2_PUBLIC_URL` is set.

## Tests

```sh
just test           # unit tests, inside the Linux images
just test othpatch  # only othpatch's tests (or games)
just test-tools     # only the tests of the Python tools, without Docker
just test-win       # the Windows unit tests, under wine
```

Unit tests live in `tests/`. They need nothing but the source, and they
run in CI. Each image runs the tests of its programs. The tests of
core/base, core/json, core/io and the tools run in both images, except
in `just test`: it runs the two images at the same time and runs these
tests in the media image only.

The Windows images build the unit tests too, but cannot run them.
`just test-win` builds them and runs them under wine, or only prints a
message when wine is not installed. CI runs them on Windows.
`tools/run_tests` runs them outside the image, from the test list that
Meson writes into each build directory. The tests of the Python tools
do not run on Windows.

The graphics pixel tests also need an OpenGL driver. Run the built test
outside Docker, with SDL's offscreen video driver. The second command
checks the same renderer through OpenGL ES 3.0:

```sh
SDL_VIDEODRIVER=offscreen build/linux/debug/tests/test_gfx_gl
SDL_VIDEODRIVER=offscreen TEST_GFX_ES=1 \
    build/linux/debug/tests/test_gfx_gl
```

These tests use generated images and do not need game data. The replay
comparisons need private cases and original data, so they run apart
from `just test` (see "Replay tools").

## Checks

[prek](https://github.com/j178/prek) runs the checks, configured in
`.pre-commit-config.yaml`. It installs the versions of the tools that
the configuration names, so that every machine and CI check alike.
clang-format formats the C code (`.clang-format`). ruff formats and
checks the Python tools (`ruff.toml`). actionlint checks the GitHub
workflows. The scripts in `tools/lint/` check the commit messages, the
versions, the changelogs and the SPDX licence lines.

```sh
prek install    # once: checks every commit and commit message
prek -a         # runs them on the whole tree
just format     # formats the C code and the Python tools
```

The style and commit rules the checks enforce are in
[CONTRIBUTING.md](../CONTRIBUTING.md).

## Packaging

```sh
just package-linux release
just package-win release
just package-mac release    # on a Mac
```

This writes one zip per game to `build/packages/`:

- `o<game>-<version>-Linux.zip`: `o<game>.AppImage`, a single
  executable file with everything inside.
- `o<game>-<version>-Windows.zip`: `o<game>.exe`.
- `o<game>-<version>-macOS.zip`: `o<game>.app`, one universal bundle
  for Apple silicon and Intel Macs. It has an ad-hoc signature only.

Each zip also holds othpatch and the game's readme, which is inside the
bundle on macOS. `tools/packaging/<game>/` holds the readme, icons and
desktop entry of a game's packages. No package contains a file of the
original game.

The packages ship the othpatch that `src/<game>/othpatch.txt` names.
Packaging downloads it as the release index gives it and checks it
against the SHA-256 digest in the index. The index is the one of the
build option `releases_url`, or the one that the environment variable
`RELEASE_INDEX_URL` names. When the index has no such othpatch, the
packages carry the othpatch built from the same commit, and packaging
says so. Outside a release build, the same happens with a warning when
the index cannot be read. A release build always ships a released
othpatch.

`just package-othpatch-linux release` and its `-win` and `-mac`
counterparts write othpatch's executable as its release names it:
`othpatch-<version>-Linux`, `othpatch-<version>-Windows.exe`, and
`othpatch-<version>-macOS`, one universal executable with an ad-hoc
signature. A development version holds a `-` instead of the `+`, as
`0.1.0-dev-abcdef0`, so that the name is safe in a URL.

Each executable carries its program's icon and version: in the Windows
file properties, the AppImage's desktop entry and the bundle's
Info.plist. `--help` prints the version too.
`tools/packaging/make_icons` renders the icons from
`tools/packaging/<program>/icon.png`, a square image of at least 1024
pixels. The icons are committed, so the build
needs no converter.

## Docker images

Each target platform has three images, defined under
`tools/docker/<platform>/<image>/`:

- `base`: the compiler, Meson, Ninja, ccache, and SDL3 and zlib, which
  every program links.
- `media`, built on `base`: SDL3_ttf with FreeType and HarfBuzz, and
  FFmpeg. The games, the replay runner and their tests build in it.
- `othpatch`, built on `base`: curl, and mbedTLS on Linux. othpatch and
  its tests build in it, so othpatch never links FFmpeg or SDL3_ttf.

On Linux the images are based on AlmaLinux 8, with clang. Its glibc
2.28 lets the executables run on older distributions. The media image
also has appimagetool for the AppImages. On Windows the images have
MinGW-w64 for x86-64 and use the Meson cross file
`tools/docker/win/meson_cross_x86_64.txt`. The media image also has
MinGW-w64 for i686 with SDL3 and zlib, for the capture tools. The
dependencies are static libraries, and the Windows executables link the
MinGW runtime statically, so they need no DLLs beyond those of Windows.

FFmpeg supports PNG, MJPEG and BMP images, and WAV, Ogg Vorbis, FLAC and
MP3 audio. No GPL component is enabled, so its licence stays LGPL. curl
supports HTTP and HTTPS only. Its TLS is mbedTLS on Linux and macOS and
Schannel on Windows. curl has no CA bundle of its own; othpatch uses the
system's certificates.

`tools/docker/pins` holds the version and SHA-256 digest of every
library and tool that the images and `tools/mac/build_deps` download.
`tools/docker/image` gives each Dockerfile the pins that it declares as
ARGs, and names the images. A tag is the date of the last commit that
changed the image's Dockerfile or its parent's, and a hash of the
Dockerfile, its pins and the parent's tag. So a change to the base image
retags the media and othpatch images, and a change to one pin retags
only the images that use it.

The images are pushed to `ghcr.io/cirno-labs/`, so CI and most
developers pull them instead of building them. In GitHub Actions the
prefix is the owner of the repository instead, so a fork builds and
pulls its own images. An owner name that Docker does not accept in an
image name gets local images instead: nothing is pushed, and CI keeps
each image in the Actions cache under its tag.

```sh
just image-linux     # build the three Linux images locally
just image-win
```

Change a Dockerfile or a pin, then rebuild and push the new images
before the change lands, or CI builds them from scratch on every run.
The "Build and push the Docker images" workflow pushes all images.

## macOS

Docker cannot produce macOS executables, so a Mac builds natively. It
needs the Xcode command line tools, Meson, Ninja and pkg-config
(`brew install meson ninja pkgconf`):

```sh
just deps-mac               # once: the libraries, for arm64 and x86-64
just build-mac release      # into build/mac-<arch>/
just test-mac
```

`tools/mac/build_deps` builds the libraries in the sets of the Docker
images, at the versions of `tools/docker/pins` and with the options of
the Linux Dockerfiles, into `build/mac-deps/<arch>/<set>/`. The games
build against the base and media sets, othpatch against the base and
othpatch sets.

## Building without Docker

Possible, not supported. You need a C23 compiler (clang 18 or GCC 14 or
newer), Meson 1.4 or newer, Ninja, pkg-config, SDL3 3.2 or newer,
SDL3_ttf 3.2.2 or newer, and the FFmpeg 7.1 development libraries
(avcodec, avformat, avutil, swresample and swscale). Then:

```sh
meson setup build/native
meson compile -C build/native
```

`tools/docker/pins` and the Dockerfiles are the reference for the
versions and build options. Builds that differ from them may behave
differently, and bugs that only appear there may not be fixed.

## Replay tools

```sh
just replay::build <game>
```

This cross-builds a game's capture tools, which capture replay test
cases from the original game. They are used locally and never packaged
or released. Each game's own notes describe how to capture with them.

`th06replay` compares captured calc rows with headless playback. It is
built with the games and never packaged. Pass the original data, the
player's replay input and its captured case:

```sh
build/linux/debug/th06replay --data <data-dir> --replay <input.rpy> \
    <case.otrc>
just replay::run <game> <case-or-dir> <data-dir> <replay-dir>
just replay::run-fast <game> <case-or-dir> <data-dir> <replay-dir> \
    [jobs]
```

`run-fast` uses a release build and runs one process per case, as many
at once as there are CPUs unless `jobs` says otherwise. It prints each
case's result and the output of the failed ones.

`--replays <dir>` matches each case to a `.rpy` with the same basename.
`--break-on-diff` stops in the debugger at the first differing calc.
`--break-at N` stops at calc N; calc numbers start at 0. Cases and
replay inputs stay outside the repository. Original files are only
read.

The runner reports skipped columns and the first calc with any
difference. Its [public header](../replay/th06/runner/run.h) describes
the supported capture format and the checks for full replay coverage.

## Releasing

This section is for maintainers.

Each program, a game or othpatch, has its own version in
`src/<program>/version.txt`: the version of its current or next
release, such as `1.2.0`. A release build reports that version. Every
other build adds `-dev` and the commit, as `1.2.0-dev+abcdef0`, or
`-dev` alone when git cannot name the commit, as in a Docker build of a
git worktree. The environment variable `OPENTOUHOU_RELEASE=1` makes a
build a release build. `just version th06` prints the version.

Each program has its own changelog, `docs/<program>/CHANGELOG.md`,
which collects the changes for players under `## Unreleased` as they
land. core has no version: a change in core goes into the changelog of
each game it affects. `tools/shared/changelog.py` describes the format:
a released version has the heading `## X.Y.Z (YYYY-MM-DD)`, and each
entry starts with `Added`, `Changed`, `Fixed` or `Removed`.

Two checks keep the changelogs in order. `tools/lint/check_changelog`
checks the format, and it fails when a commit changes or removes a
released section. To correct a released section on purpose, set
`CHANGELOG_AMEND` to its version, such as
`CHANGELOG_AMEND=1.2.0 git commit`; a comma separates more versions.
`tools/lint/check_versions` makes sure that each changelog has a section
for the version in `version.txt`, or unreleased changes when that
version has no release yet, and that each `othpatch.txt` holds a
version.

A release has two steps: a commit that sets the versions, and a manual
run of the "Publish the releases" workflow. To release th06 1.2.0:

```sh
tools/bump th06 1.2.0 --commit
git push origin HEAD
gh workflow run release.yml --ref main
```

`tools/bump` writes the version into `version.txt` and turns th06's
`## Unreleased` heading into `## 1.2.0 (YYYY-MM-DD)` with the date of
the day in UTC. The workflow publishes the bump commit, so the date is
set there. To release othpatch and make every game ship it, each game
in a new patch release:

```sh
tools/bump othpatch 0.2.0 --with-games --commit
```

The workflow publishes every program whose `version.txt` names a
version that the release index does not have, from the commit it runs
on. A version without a changelog section waits until `tools/bump`
writes it. The workflow builds and publishes othpatch first, then the
games, whose packages take the othpatch that they ship from the index.
For each program, `tools/release_index` uploads the files to the R2
bucket and then adds the release to the index, and the workflow pushes
the tag `<product>-vX.Y.Z`, such as `oth06-v1.2.0`. A run without
unreleased versions does nothing. There are no GitHub releases.

The bucket, the release index and the tags name a game by its product
name, the name of its executable: `oth06` for th06. othpatch keeps its
name. The source tree and the arguments of the tools, such as
`tools/bump th06`, keep the game id.

A run with the input `replace` also publishes again each version that
was published less than 24 hours ago, from the commit it runs on, to
correct a broken release. It replaces the version's files, checksums
and index entry and moves its tag. A version published 24 hours ago or
more stays as it is: release a new version instead. Players who
installed the replaced version get no update to the new files.

The R2 bucket holds the downloads:

- `<product>/<version>/<file>`: the files of a release, such as
  `oth06/0.1.1/oth06-0.1.1-Linux.zip`. A published version changes
  only through `replace`.
- `snapshot/<file>`: the development snapshot.
- `releases.json`: the release index, written after the files.
  `tools/shared/release_index.py` describes its format. othpatch and
  packaging read it.

Every push to `main` replaces the development snapshot, when the
repository variable `PRERELEASE_ENABLE` is `true`. It holds the game
packages with the othpatch that they ship, the othpatch of the commit,
and each program's unreleased changes.

The manual "Delete the releases" workflow deletes every release from
the bucket and the index and keeps the snapshot. It asks for the word
`DELETE`. The tags stay. "Publish the releases" and "Delete the
releases" run only on `main`.

The workflows need these repository settings:

- Variables `R2_ACCOUNT_ID` and `R2_BUCKET`: the bucket.
- Variable `R2_PUBLIC_URL`: the public URL of the bucket, without a
  `/` at the end.
- Secrets `R2_ACCESS_KEY_ID` and `R2_SECRET_ACCESS_KEY`: a key that can
  read and write the bucket. They belong to the environment `release`,
  which you create in the repository settings, so that its protection
  rules, such as required reviewers, apply to every job that writes the
  bucket.
- Variable `SITE_REPOSITORY` and secret `SITE_DISPATCH_TOKEN`: the
  download site's repository and a token that may send it a
  `repository_dispatch` event. After each successful snapshot, release
  or deletion, "Tell the site about new releases" asks the site to
  rebuild. Without them the site rebuilds on its own schedule.
