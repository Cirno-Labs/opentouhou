opentouhou th01
===============

oth01 is an unofficial native port of the PC-98 game 東方靈異伝 ~ Highly
Responsive to Prayers (Touhou 1) for Windows, Linux and macOS, faithful
frame for frame. It needs your own copy of the game data to run.


Installing
----------

Copy the files of the original game out of its PC-98 disk image into
one folder, then put the files of the package beside them, next to
TOUHOU.DAT:

- Windows: oth01.exe and othpatch.exe
- Linux: oth01.AppImage and othpatch. If they do not start, make them
  executable.
- macOS: oth01.app and othpatch

Then start the program. It checks that REIIDEN.EXE is the supported
version. The program never changes the original game files. Keep
othpatch beside the program, under its supplied name, so that it can
find and update the game.

In the commands below, use oth01.exe and othpatch.exe on Windows,
./oth01.AppImage and ./othpatch on Linux, or ./othpatch on macOS.

On macOS, the program has no developer signature, and macOS runs a
downloaded program from a temporary copy that cannot see the files
beside it. Before the first start, open Terminal in the folder of the
game and type:

    xattr -dr com.apple.quarantine oth01.app othpatch

To keep the program in a different folder, give it the folder of the
game when you start it:

    oth01 --data <folder of the game>

On macOS, the program inside oth01.app takes the arguments:

    oth01.app/Contents/MacOS/oth01 --data <folder of the game>

To see all arguments and the folders that the program uses, type:

    oth01 --help

The original game draws its text from the font ROM of the PC-98. The
program draws that text in the bundled Shinonome 16 font. To use a dump
of the ROM instead, put it in the opentouhou/fonts/ folder of the game
folder: FONT.ROM as T98Tools makes it, or NEC.BMP, ANEX86.BMP or
FREECG98.BMP in Anex86's format. The program takes the first that it
finds, in that order.

The package also includes oth01_readme.txt and oth01_licenses.txt.
Their names keep them separate from the original game's documents.


Your files
----------

The program keeps your files in your user folder:

- config.ini, the settings
- REYHIES.DAT, REYHINO.DAT, REYHIHA.DAT and REYHILU.DAT, the scores of
  the four difficulties
- screenshots/
- oth01.log, the log of the last start. Send it with a bug report.

The user folder is:

- Windows: %APPDATA%\opentouhou\th01\
- Linux: ~/.local/share/opentouhou/th01/
- macOS: ~/Library/Application Support/opentouhou/th01/

On Linux, $XDG_DATA_HOME replaces ~/.local/share when it is set.

The score files have the format of the original game. On the first
start without a config.ini, the program takes valid settings from the
REIIDEN.CFG of the game folder. For each difficulty without a score
file in the user folder, it reads the score file of the game folder.
New scores go only to the user folder; the original settings and
scores stay unchanged.

To use a different user folder, start the program with:

    oth01 --user-dir <folder>


Portable mode
-------------

Put an empty file with the name portable.txt beside the program. The
program then keeps your files in the userdata/ folder beside it. Use
this mode to keep the game and your files together, for example on a
USB drive. --user-dir still overrides the user folder.


Keys
----

- Left and Right, or keypad 4 and 6: move Reimu.
- Z: shoot. X: swing the gohei; combine it with movement for kicks.
- Quickly tap Z twice and X twice to use a bomb, when one is available.
- Return or Z: confirm a menu choice. Up and Down select a row;
  Left and Right change an option. Escape returns from menus.
- Escape during play: pause or resume. The pause menu also lets you
  leave the current game after confirmation.
- Return confirms high-score name entry; use the arrows to select
  characters and Z or X to edit the name.
- Z, X, Return or Escape skips the opening logo.
- F11 or Alt+Enter: switch between window and full screen.
- F12 or Print Screen: save a screenshot.

On a gamepad, the directional pad or left stick moves, south shoots,
east swings the gohei, start confirms and back pauses or returns.
Change bindings in the [input.keyboard], [input.gamepad] and
[input.joystick] sections of config.ini.


Settings
--------

config.ini in the user folder holds the settings: the video, screenshot
and key settings, and the options of the game. The Options menu of the
game changes the [game] settings.

- [game] difficulty: easy, normal, hard or lunatic. It is normal by
  default.
- [game] music: fm (the default) or off. off silences the music of the
  menus and the music and sound effects of play; the music room and the
  endings still play their music.
- [game] bombs: 0 to 5, 1 by default.
- [game] lives_extra: the extra lives at the start, 0 to 4, 2 by
  default.
- [video] fullscreen: starts the program in full screen. It is false by
  default. F11 and Alt+Enter change it.
- [video] scaling: sharp, smooth or integer. sharp (the default) shows
  square pixels as large as the window allows, smooth scales with soft
  edges, and integer shows square pixels at a whole multiple of 640x400,
  with a black border around them.
- [fixes] music_restart: starts a music room track once each time the
  confirm key is pressed. The original restarts the track on every
  frame that the key is held. It is on by default. Set it to false to
  play the music room as the original game does.
- [screenshot] resolution: native (the default) or window. format: png
  (the default) or jpg. jpeg_quality: 1 to 100, 90 by default.


Updates
-------

Keep othpatch beside the game, under its supplied name. It keeps oth01
and itself up to date. Each run checks for newer releases of the games
beside it and of othpatch, and asks before installing them:

    othpatch

To accept available updates without a question:

    othpatch --update

Development builds are never replaced by release updates. Updates keep
your settings and scores. A game's bundled othpatch replaces the
installed release only when it is newer.

For folder details, or all arguments:

    othpatch --verbose
    othpatch --help


Credits
-------

- ZUN: the original game, characters, art and music.
- SDL, SDL_ttf, FreeType, HarfBuzz, FFmpeg, libjpeg-turbo, zlib, curl
  and Mbed TLS: libraries.
- Shinonome 16 (東雲フォント): the bundled text font, by Yasuyuki
  Furukawa and the /efont/ project.
- Regulus: MDRV2 and its documentation; HertzDevil: the MDT decoder.
- libvgm contributors, including Jarek Burczynski, Tatsuyuki Satoh,
  Couriersud and Hiromitsu Shioya: the OPNA sound cores.

Special thanks to the projects behind this port:

- nmlgc and the ReC98 contributors: the reconstruction of TH01's source
  code, whose work made this port possible.
  https://github.com/nmlgc/ReC98
- N0zoM1z0: the th01 repository this port was based on.
  https://github.com/N0zoM1z0/th01
- Koizuka and contributors: master.lib; やなぎさわ and BERO: PiLoad. The
  port reproduces both.

Source and documentation:
https://github.com/Cirno-Labs/opentouhou
Full licensing details available in oth01_licenses.txt.

This is an unofficial port, not endorsed by ZUN.
