# opentouhou

Unofficial cross-platform ports of the Touhou Project games, starting
with 東方紅魔郷 ~ the Embodiment of Scarlet Devil (Touhou 6).

Each port is written from a matching decompilation of the original game:

- [th06](https://github.com/Cirno-Labs/th06)
- [th07](https://github.com/Cirno-Labs/th07)

The decompilations reproduce the original executables byte for byte.
This repository turns their source into a native game for Linux, macOS
and Windows, free of the matching constraints.

This repository contains no game code or data. You need your own copy of
each game.

## Status

th06 is playable from the title screen to the ending: every stage with
its dialogue, sound and music, the menus, the result screen with scores
and replays, the music room and the endings. Replays recorded with the
original game play back in the port; the 17 world-record replays match
the original frame by frame.

Next: English and other translations, the fixes and optional changes
known from the community's patches, then th07.

## Playing

Download the package for your system from the releases, and put the
executable into the folder of your copy of th06:

- Windows: `oth06.exe` from `oth06-<version>-Windows.zip`
- Linux: `oth06-<version>-Linux.AppImage`
- macOS: `oth06.app` from `oth06-<version>-macOS.zip`

The executables start with an "o" so that they never replace the
original's `th06.exe`. To keep the port elsewhere, start it with
`--data <folder>`. `oth06 --help` lists the options and shows where the
settings, scores and replays go. The data folder is only read.

Settings are in `config.ini` in that user folder: the video, screenshot
and key settings, and the game's own options.

## Features

- Native builds for Linux, macOS and Windows, 64-bit, through SDL3
  instead of Direct3D 8, DirectInput, DirectSound and the Win32 API
- Game logic at a fixed 60 Hz, as in the original, with motion
  interpolated between ticks on faster displays (can be turned off)
- Replays recorded with the original game play back on every platform:
  the game logic reproduces the original floating-point math
- Fullscreen and window modes, rebindable keyboard and gamepad controls,
  screenshots

## Layout

```
src/core/   shared by every game: window, rendering, input, audio,
            text, file IO, settings, frame pacing
src/stg/    shared by the shooting games: archives, random numbers
src/th06/   one directory per game
replay/     tools that capture cases from the original game and compare
            the port against them
tools/      build, packaging and release tools
docs/       documentation: ARCHITECTURE.md, BUILDING.md, TH06.md
```

[docs/BUILDING.md](docs/BUILDING.md) describes how to build, test and
release.

## Legal

Touhou Project is by ZUN / Team Shanghai Alice. This is an unofficial
fan project. It contains no assets or code from the original games, and
it is not affiliated with or endorsed by ZUN or Team Shanghai Alice.

## License

The source in this repository is licensed under the GNU General Public
License, version 3 only (`GPL-3.0-only`), see [`LICENSE`](LICENSE). The
license covers our code, not the original games or their data.
