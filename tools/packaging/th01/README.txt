opentouhou th01
==============

oth01 is an unofficial native port of Touhou 1, Highly Responsive to
Prayers. One program runs the logo, title, menus, options, music room,
both gameplay routes, endings, verdicts and high-score registration.

This development build has no audible music or sound effects, including
in the music room. Sound playback through an audio device is unfinished.
Full original-game playback and timing parity have not been established.
Linux and Windows builds have local checks. macOS runtime behavior has
not been verified.


Installing
----------

You need your own original game data. It is not included. Use the full,
flat game-data folder: TOUHOU.DAT alone is not enough. Keep the original
.DAT, .GRP, .GRF, .BOS, music and other game files together. A disk image
is not a data folder; this program does not mount disk images.

Put the package beside the original game files:

- Linux: oth01.AppImage and othpatch. Make them executable if needed.
- Windows: oth01.exe and othpatch.exe.

Or keep the program elsewhere and pass the original data folder:

    oth01 --data /path/to/th01

On Linux, use ./oth01.AppImage in place of oth01 in these commands.
On Windows, use oth01.exe. No DOS executables or emulator are needed.
The program reads the original files and never changes them.

To see the arguments and the folders in use:

    oth01 --help


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

A gamepad uses the directional pad or left stick for directions, south
for Z, east for X, start for Return and back for Escape.


Your files and settings
-----------------------

The program saves its files separately from the original game:

- config.ini: video, input and game settings.
- REYHIES.DAT, REYHINO.DAT, REYHIHA.DAT and REYHILU.DAT: the four
  difficulty tables, in the original score format.
- screenshots/: screenshots.
- oth01.log: the log of the last start; include it in bug reports.

The user folder is:

- Linux: ~/.local/share/opentouhou/th01/
- Windows: %APPDATA%\opentouhou\th01\
- macOS: ~/Library/Application Support/opentouhou/th01/

On Linux, $XDG_DATA_HOME replaces ~/.local/share when set. To choose a
user folder, pass --user-dir /path/to/userdata.

On first launch, valid REIIDEN.CFG settings are read from the data folder
if no native config.ini exists. Each score table is read from the data
folder if it has no saved table in the user folder. New scores go only
to the user folder. Original settings and scores stay unchanged.

Use Options to select difficulty, music mode, bombs and extra lives.
The music setting currently produces no audible output. config.ini
also holds the video scaling, screenshot and input settings. In [video],
scaling can be sharp, smooth or integer.

For portable mode, put an empty portable.txt beside the executable.
The user folder then becomes userdata/ beside it; shared patches use
patches/ beside it. --user-dir overrides the user folder. Keep portable
files outside a read-only original-data folder.


Updates and credits
-------------------

othpatch is the shared updater supplied with the package. Its --help
lists its commands. This local development package has no configured
public update service. TH01 translations and patch playback have not
been verified.

The original game is by ZUN. This port is not endorsed by ZUN.
The program uses SDL, SDL_ttf, FreeType, HarfBuzz, FFmpeg, libjpeg-turbo,
zlib, the IPAGothic and IPAPGothic fonts and imported OPNA emulation
code. See THIRD_PARTY.txt and the accompanying license texts for source
provenance and terms.
Our code is free software under the GNU General Public License,
version 3 only. Imported code retains its individual terms.

Source code, documentation and licenses:
https://github.com/Cirno-Labs/opentouhou
