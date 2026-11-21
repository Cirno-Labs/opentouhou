# opentouhou

Unofficial cross-platform ports of the Touhou Project games.

Downloads, release notes and help: <https://opentouhou.org/>

## Games

- [東方紅魔郷 ~ the Embodiment of Scarlet Devil](https://opentouhou.org/oth06-manual.html)
  (th06) can be played from start to finish, and its 17 world-record
  replays play back exactly as in the original.
- [東方妖々夢 ~ Perfect Cherry Blossom](https://opentouhou.org/oth07-manual.html)
  (th07) can be played from start to finish, Extra and Phantasm
  included. Its 36 world-record replays play back exactly as in the
  original, and so do replays recorded with VsyncPatch.
- [東方靈異伝 ~ Highly Responsive to Prayers](tools/packaging/th01/oth01_readme.txt)
  (th01), a development snapshot, can be played from start to finish,
  both routes and the endings included. Its test runs match the original
  PC-98 game frame by frame in DOSBox-X.

Each port is written from a matching decompilation, which reproduces the
original executable byte for byte. This repository turns that source
into a native game for Linux, macOS and Windows, free of the matching
constraints. It contains no game code or data: you need your own copy of
each game.

othpatch, which comes with each game, downloads the English and other
translations of thpatch.net. It also updates the games and itself.

Next: more games, and the fixes and optional changes known from the
community's patches.

## Playing

Download the zip of your game for your system from
<https://opentouhou.org/>, `oth<NN>-<version>-<system>.zip`. It holds the
game, `oth<NN>.exe`, `oth<NN>.AppImage` or `oth<NN>.app`, and othpatch.
Put both into the folder of your copy of the game. The name starts with
an "o" so that it never replaces the original's `th<NN>.exe`, and the
data folder is only read.

Each game's manual, linked above and also in the zip as
`oth<NN>_readme.txt`, tells where your files go and describes portable
mode, the keys, the settings and othpatch.

## Features

- Native builds for Linux, macOS and Windows
- SDL3 in place of Direct3D 8, DirectInput, DirectSound and Win32
- Game logic at a fixed 60 Hz, as in the original
- Smooth motion on fast displays, which can be turned off
- Sharp, smooth or integer scaling
- Window or fullscreen, switched with F11 or Alt+Enter
- Rebindable keyboard, gamepad and joystick controls
- Screenshots in PNG or JPEG
- Replays and scores in the original's formats
- Original replays play back exactly on every platform
- Translations from thpatch.net through othpatch
- Updates of the games and othpatch through othpatch
- Portable mode
- An optional input lag fix in th07, as with VsyncPatch

## Layout

```
src/core/      shared by every game: rendering, input, audio, text,
               file IO, settings
src/shell/     the program around core: folders, log file, window,
               main loop, frame pacing
src/stg/       shared by the shooting games: archives, random numbers
src/th<NN>/    one directory per game
src/othpatch/  the patch downloader and updater
tests/         unit tests
replay/        tools that capture cases from the original game and
               compare the port against them
tools/         build, packaging and release tools
docs/          documentation: ARCHITECTURE.md, BUILDING.md, a TH<NN>.md
               per game and a changelog per program
```

## Building

You need Docker, just and Python 3.11 or newer:

```sh
just build-linux                 # build every game and othpatch
just test                        # run the unit tests
just run th06 --data <folder>    # build th06 and start it
```

[docs/BUILDING.md](docs/BUILDING.md) describes how to build, test and
release. [CONTRIBUTING.md](CONTRIBUTING.md) describes how to send a
change and the rules that it follows.

## Legal

Touhou Project is by ZUN / Team Shanghai Alice. This is an unofficial
fan project. It contains no assets or code from the original games, and
it is not affiliated with or endorsed by ZUN or Team Shanghai Alice.

## License

The source in this repository is licensed under the GNU General Public
License, version 3 only (`GPL-3.0-only`), see [`LICENSE`](LICENSE). The
license covers our code, not the original games or their data.
