opentouhou
==========

This program is an unofficial port of a Touhou Project game to Linux,
macOS and Windows. oth07 is 東方妖々夢 ~ Perfect Cherry Blossom
(Touhou 7).

oth07 is in development and you cannot play it yet. It starts and reads
the data of the game, but it shows no game. This package is a
development build for testing.

The program needs the data of the original game. The data is not
included. You must have your own copy of the game.


Installing
----------

Put the files of the package into the folder of the original game,
beside its .dat files:

- Windows: oth07.exe and othpatch.exe
- Linux: oth07.AppImage and othpatch. If they do not start, make them
  executable.
- macOS: oth07.app and othpatch

Then start the program. The program never changes the files in that
folder. On macOS, the program has no developer signature, so macOS asks
before the first start: open oth07.app from its context menu.

To keep the program in a different folder, give it the folder of the
game when you start it:

    oth07 --data <folder of the game>

To see all arguments and the folders that the program uses, type:

    oth07 --help


Your files
----------

The program keeps your files in your user folder:

- config.ini, the settings
- oth07.log, the log of the last start. Send it with a bug report.

The user folder is:

- Windows: %APPDATA%\opentouhou\th07\
- Linux: ~/.local/share/opentouhou/th07/
- macOS: ~/Library/Application Support/opentouhou/th07/

On Linux, $XDG_DATA_HOME replaces ~/.local/share when it is set.

To use a different user folder, start the program with:

    oth07 --user-dir <folder>


Portable mode
-------------

Put an empty file with the name portable.txt beside the program. The
program then keeps your files in the userdata/ folder beside it, and
reads patches from the patches/ folder beside it.


othpatch
--------

othpatch in this package does not know th07 yet: it does not get
translations for th07 and does not update oth07. Do not use it with
this package.


Credits
-------

The original game is by ZUN (Team Shanghai Alice).

The program uses SDL, SDL_ttf, FreeType, HarfBuzz, FFmpeg, zlib and the
IPAGothic and IPAPGothic fonts.

Source code, documentation and licences:
https://github.com/Cirno-Labs/opentouhou

This program is free software under the GNU General Public License,
version 3 only.
