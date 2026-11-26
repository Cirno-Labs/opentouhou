opentouhou th02
===============

oth02 is an unofficial native port of the PC-98 game 東方封魔録 ~ the
Story of Eastern Wonderland (Touhou 2). It needs your own copy of the
game data to run.

This development build includes normal and Extra play, endings and
score saving. Original-game parity verification is in progress.


Installing
----------

Keep the original game files together in one folder. Put the files of
the package beside them, next to TOUHOU.DAT and 東方封魔.録:

- Windows: oth02.exe and othpatch.exe
- Linux: oth02.AppImage and othpatch. If they do not start, make them
  executable.
- macOS: oth02.app and othpatch

Then start the program. It keeps new settings and scores in your user
folder and reads the original game files without changing them. Keep
othpatch beside the program, under its supplied name.

In the commands below, use oth02.exe and othpatch.exe on Windows,
./oth02.AppImage and ./othpatch on Linux, or ./othpatch on macOS.

On macOS, the program has no developer signature, and macOS runs a
downloaded program from a temporary copy that cannot see the files
beside it. Before the first start, open Terminal in the game folder
and type:

    xattr -dr com.apple.quarantine oth02.app othpatch

To keep the program in a different folder, give it the game folder:

    oth02 --data <folder of the game>

On macOS, the program inside oth02.app takes the arguments:

    oth02.app/Contents/MacOS/oth02 --data <folder of the game>

--data also takes a zip of the game folder, or a PC-98 hard disk image
(.hdi), bare or in a zip. The program reads these without unpacking
them.

To see all arguments and the folders that the program uses, type:

    oth02 --help

The program draws PC-98 text in the bundled Shinonome 16 font. To use
your own font ROM dump, put it in opentouhou/fonts/ in the game
folder: FONT.ROM as T98Tools makes it, or NEC.BMP, ANEX86.BMP or
FREECG98.BMP in Anex86's format. The program takes the first that it
finds, in that order.

The package also includes oth02_readme.txt and oth02_licenses.txt.
Their names keep them separate from the original game's documents.


Your files
----------

The program keeps your files in your user folder:

- config.ini, the settings
- HUUHI.DAT, the scores and clear records
- screenshots/
- oth02.log, the log of the last start. Send it with a bug report.

The user folder is:

- Windows: %APPDATA%\opentouhou\th02\
- Linux: ~/.local/share/opentouhou/th02/
- macOS: ~/Library/Application Support/opentouhou/th02/

On Linux, $XDG_DATA_HOME replaces ~/.local/share when it is set.

On the first start without a config.ini, the program imports valid
settings from HUUMA.CFG and scores from HUUHI.DAT in the game folder,
when present. Existing user scores take precedence.
It creates HUUHI.DAT in the user folder and writes new scores there.
The score file has the original game's format. The original settings
and scores stay unchanged.

To use a different user folder, start the program with:

    oth02 --user-dir <folder>


Portable mode
-------------

Put an empty file named portable.txt beside the program. The program
then keeps your files in userdata/ beside it. --user-dir overrides
this folder.


Keys
----

- Arrow keys or keypad direction keys: move Reimu.
- Z or Space: shoot. X: use a bomb when one is available.
- Return, Z or Space: confirm a menu choice. Up and Down select a
  row; Left and Right change an option. Escape returns from menus.
- Left and Right choose a shot style before play.
- Escape during play: pause. Press it again to resume. The pause
  menu also lets you leave the current game after confirmation.
- During high-score name entry, the arrows select a character and
  Return, Z or Space enters it. X erases. Escape finishes the name.
- Z, X, Return or Escape skips the opening logo or ends an idle demo.
- F11 or Alt+Enter: switch between window and full screen.
- F12 or Print Screen: save a screenshot.

On a gamepad, the directional pad or left stick moves, south shoots,
east uses a bomb, start confirms and back pauses or returns. Change
bindings in the [input.keyboard], [input.gamepad] and [input.joystick]
sections of config.ini.

Clear the normal game with each of the three shot styles without
continuing to unlock Extra.


Settings
--------

config.ini holds the video, screenshot, input and game settings.
The Options menu changes the [game] settings.

- [game] difficulty: easy, normal, hard or lunatic; normal by default.
- [game] music: fm by default, or off. A legacy midi value uses FM
  playback.
- [game] bombs: 0 to 3, 3 by default.
- [game] lives: spare lives at the start, 0 to 4, 2 by default.
- [game] reduce_effects: reduces visual effects when true; false by
  default.
- [video] fullscreen: false by default. F11 and Alt+Enter change it.
- [video] scaling: sharp, smooth or integer. sharp is the default;
  smooth softens edges, and integer uses whole multiples of 640x400
  with a black border.
- [fixes] bomb_transition_overread: omits the missing final bomb
  transition mask, enabled by default. false draws the memory that the
  original reads in its place.
- [screenshot] resolution: native or window, native by default.
  format: png or jpg, png by default. jpeg_quality: 1 to 100, 90 by
  default.


Updates
-------

Keep othpatch beside the game, under its supplied name. Each run
checks for newer releases of the games beside it and of othpatch,
and asks before installing them:

    othpatch

To accept available updates without a question:

    othpatch --update

Development builds are never replaced by release updates. Updates
keep your settings and scores. For folder details or all arguments:

    othpatch --verbose
    othpatch --help


Credits
-------

- ZUN: the original game, characters, art and music.
- SDL, SDL_ttf, FreeType, HarfBuzz, FFmpeg, libjpeg-turbo, zlib, curl
  and Mbed TLS: libraries used by the game and updater.
- Shinonome 16 (東雲フォント): the bundled text font, by Yasuyuki
  Furukawa and the /efont/ project.
- KAJA: PMD and its documentation.
- libvgm contributors, including Jarek Burczynski, Tatsuyuki Satoh,
  Couriersud and Hiromitsu Shioya: the OPNA sound cores.

Special thanks to the projects behind this port:

- N0zoM1z0: the reconstructed TH02 source this port was based on.
  https://github.com/N0zoM1z0/th02
- nmlgc and the ReC98 contributors: the reconstruction work behind
  that source.
  https://github.com/nmlgc/ReC98
- Koizuka and contributors: master.lib; やなぎさわ and BERO: PiLoad.
  The port reproduces both.

Source and documentation:
https://github.com/Cirno-Labs/opentouhou
Full licensing details are in oth02_licenses.txt.

This is an unofficial port, not endorsed by ZUN.
