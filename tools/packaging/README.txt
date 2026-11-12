opentouhou
==========

This program is an unofficial port of a Touhou Project game to Linux,
macOS and Windows. oth06 is 東方紅魔郷 ~ the Embodiment of Scarlet Devil
(Touhou 6). You can play the full game, and save scores and replays.

The program needs the data of the original game. The data is not
included. You must have your own copy of the game.


Installing
----------

Put the program into the folder of the original game, beside its .dat
files:

- Windows: oth06.exe
- Linux: the .AppImage file. Make it executable before you start it.
- macOS: oth06.app

Then start the program. The program never changes the files in that
folder.

To keep the program in a different folder, give it the folder of the
game when you start it:

    oth06 --data <folder of the game>

To see all arguments and the folders that the program uses, type:

    oth06 --help


Your files
----------

The program keeps your files in your user folder:

- config.ini, the settings
- score.dat, the scores
- replay/, the replays
- screenshots/

The user folder is:

- Windows: %APPDATA%\opentouhou\th06\
- Linux: ~/.local/share/opentouhou/th06/
- macOS: ~/Library/Application Support/opentouhou/th06/

The files have the format of the original game. To keep your scores and
replays from the original game, copy its score.dat and replay/ folder
into the user folder.

To use a different user folder, start the program with:

    oth06 --user-dir <folder>


Portable mode
-------------

Put an empty file with the name portable.txt beside the program. The
program then keeps your files in the userdata/ folder beside it, and
reads patches from the patches/ folder beside it. Use this mode to keep
the game and your files together, for example on a USB drive.


Keys
----

F11 or Alt+Enter changes between window and full screen. F12 or Print
Screen saves a screenshot.


Translations
------------

Translations come with the othpatch tool, which is available separately.


Credits
-------

The original game is by ZUN (Team Shanghai Alice). The translations are
by the thpatch.net community, in the patch format of thcrap.

The program uses SDL, SDL_ttf, FreeType, HarfBuzz, FFmpeg, zlib and the
BIZ UDGothic font.

Source code, documentation and licences:
https://github.com/Cirno-Labs/opentouhou

This program is free software under the GNU General Public License,
version 3 only.
