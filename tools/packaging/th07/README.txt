opentouhou
==========

This program is an unofficial port of a Touhou Project game to Linux,
macOS and Windows. oth07 is 東方妖々夢 ~ Perfect Cherry Blossom
(Touhou 7). You can play the full game, and save scores and replays.

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
folder.

On macOS, the program has no developer signature, and macOS runs a
downloaded program from a temporary copy that cannot see the files
beside it. Before the first start, open Terminal in the folder of the
game and type:

    xattr -dr com.apple.quarantine oth07.app othpatch

To keep the program in a different folder, give it the folder of the
game when you start it:

    oth07 --data <folder of the game>

On macOS, the program inside oth07.app takes the arguments:

    oth07.app/Contents/MacOS/oth07 --data <folder of the game>

To see all arguments and the folders that the program uses, type:

    oth07 --help


Your files
----------

The program keeps your files in your user folder:

- config.ini, the settings
- score.dat, the scores
- replay/, the replays
- screenshots/
- oth07.log, the log of the last start. Send it with a bug report.

The user folder is:

- Windows: %APPDATA%\opentouhou\th07\
- Linux: ~/.local/share/opentouhou/th07/
- macOS: ~/Library/Application Support/opentouhou/th07/

On Linux, $XDG_DATA_HOME replaces ~/.local/share when it is set.

The files have the format of the original game. To keep your scores and
replays from the original game, copy its score.dat and replay/ folder
into the user folder.

To use a different user folder, start the program with:

    oth07 --user-dir <folder>

--user-dir moves only the user folder. The patches folder stays where
it is, so all games of one installation share it.


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


Settings
--------

config.ini in the user folder holds the settings: the video, screenshot
and key settings, and the options of the game.

- [game] show_fps: shows the frame rate outside ending scenes. It is
  enabled by default. Set it to false to hide the counter.

- [video] interpolation: smooths motion between the 60 Hz steps of the
  game on faster displays. It is on by default. Set it to false to show
  each step as the game drew it.
- [video] scaling: sharp, smooth or integer. sharp (the default) shows
  square pixels as large as the window allows, smooth scales with soft
  edges, and integer shows square pixels at a whole multiple of 640x480,
  with a black border around them.
- [fixes] replay_desync: plays original replays as they were recorded.
  The original's playback can lower the score carried into a stage by
  up to 655,350 points, and in replays recorded with VsyncPatch a shot
  held through a pause can skip a line of dialogue and change the rest
  of the run. It is on by default. Set it to false to play replays as
  the original game does. It does not change live play or recorded
  input, and playback does not rewrite the source replay while this fix
  is on.
- [fixes] input_lag: has the game act on a button press a frame
  sooner, as VsyncPatch does for the original game. It is off by
  default. Replays recorded with it are like those recorded with
  VsyncPatch, and play back in the original game as those do.
- [screenshot] resolution: native or window. format: png or jpg.
  jpeg_quality: 1 to 100.
- [patches] enabled: set it to false to turn off all patches.
  disabled: patch ids to leave out, separated by commas, for example
  thpatch/lang_de. language: the code of the language pack to use, for
  example de, or ja for the original game without a translation.


Translations and updates
------------------------

The program applies the thcrap patches in the patches folder:

- Windows: %APPDATA%\opentouhou\patches\
- Linux: ~/.local/share/opentouhou/patches/
- macOS: ~/Library/Application Support/opentouhou/patches/
- Portable mode: patches/ beside the program

The patches folder has the layout of the repos/ folder of thcrap, so a
copy of that folder works: one folder per repository, and in it one
folder per patch. The program uses one language pack and every other
patch that no patch needs, each with the patches that it needs. The
language is [patches] language. Without it, the program uses the only
language pack, or of several, the one for the language of your system.
oth07.log tells which patches apply and why.

Keep othpatch in the same folder as the program. othpatch downloads
the translations of thpatch.net into the patches folder. The first
time, it gets the translation into the language of your system. Start
othpatch, or type:

    othpatch

To get a different language, give its code, for example English:

    othpatch en

othpatch then sets [patches] language in the config.ini of the
program. Your other languages stay in the patches folder. To play the
original game without a translation, type:

    othpatch ja

To see every language and the mods that you have, type:

    othpatch --list

If you start the program with --user-dir, othpatch cannot find that
config.ini: add the line language = <code> under [patches] there.

Each time, othpatch also updates the translations and the mods that
you have. Then it looks for a newer release of othpatch and of the
program, and asks before it installs it. To install the updates
without a question, type:

    othpatch --update

To see all arguments, type:

    othpatch --help


Credits
-------

The original game is by ZUN (Team Shanghai Alice). The translations are
by the thpatch.net community, in the patch format of thcrap.

The program uses SDL, SDL_ttf, FreeType, HarfBuzz, FFmpeg, libjpeg-turbo,
zlib and the IPAGothic and IPAPGothic fonts. This software is based in
part on the work of the Independent JPEG Group.

Source code, documentation and licences:
https://github.com/Cirno-Labs/opentouhou

This program is free software under the GNU General Public License,
version 3 only.
