opentouhou th07
===============

oth07 is an unofficial native port of 東方妖々夢 ~ Perfect Cherry
Blossom (Touhou 7) for Windows, Linux and macOS, faithful down to its
replays. It needs your own copy of the game data to run.


Installing
----------

Put the files of the package into the folder of the original game,
beside its .dat files:

- Windows: oth07.exe and othpatch.exe
- Linux: oth07.AppImage and othpatch. If they do not start, make them
  executable.
- macOS: oth07.app and othpatch

Then start the program. The program never changes the original game
files. Keep othpatch beside the program, under its supplied name, so
that it can find and update the game.

In the commands below, use oth07.exe and othpatch.exe on Windows,
./oth07.AppImage and ./othpatch on Linux, or ./othpatch on macOS.

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

--data also takes a zip of the folder of the game, which the program
reads without unpacking it.

To see all arguments and the folders that the program uses, type:

    oth07 --help

The program draws text in MS Gothic, as the original does, when the
system has it, and in the bundled IPAGothic otherwise. To use
another font file, put it in the opentouhou/fonts/ folder of the game
folder: a font there replaces the font of the same name.

The package also includes oth07_readme.txt and oth07_licenses.txt.
Their names keep them separate from the original game's documents.


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

- Arrow keys or keypad 2, 4, 6 and 8: move or select a menu item.
- Z: shoot or confirm. Return also confirms a menu choice.
- X: use a bomb or go back in a menu.
- Shift: move slowly while held.
- Escape: pause or open the menu.
- Ctrl: skip dialogue while held.
- F11 or Alt+Enter: switch between window and full screen.
- F12 or Print Screen: save a screenshot.

On a gamepad, the directional pad or left stick moves, south shoots,
east bombs, west focuses, start opens the menu and north skips. Change
bindings in the [input.keyboard], [input.gamepad] and [input.joystick]
sections of config.ini.


Settings
--------

config.ini in the user folder holds the settings: the video, screenshot
and key settings, and the options of the game.

- [game] show_fps: shows the frame rate outside ending scenes. It is
  enabled by default. Set it to false to hide the counter.
- [game] shoot_focuses: shoot held on a gamepad or joystick for 10
  frames also focuses. It is on by default. Set it to false to focus
  only with the focus button.
- [game] slowdown: slows the game down where many bullets show. A game
  in this mode saves no score and no replay. It is off by default. The
  options menu also changes it.
- [game] effects_quality: 0 to 2, 2 by default. Below 2, the falling
  cherry petals do not draw.
- [video] fullscreen: starts the program in full screen. It is false by
  default. F11 and Alt+Enter change it.
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
- [screenshot] resolution: native (the default) or window. format: png
  (the default) or jpg. jpeg_quality: 1 to 100, 90 by default.
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

Keep othpatch beside the game, under its supplied name. It downloads
thpatch.net translations and updates your installed patches. Restart
the game to use changed patches.

On the first run it selects your system language. Later runs keep your
selection and update your patches:

    othpatch

Choose another language, or return to the original Japanese:

    othpatch en
    othpatch ja

This sets [patches] language in config.ini; other language packs stay
installed. With a custom --user-dir, set language = <code> under
[patches] in that config.ini yourself.

List available languages and installed mods:

    othpatch --list

Install a mod by repository and patch id, or remove it:

    othpatch nmlgc/aero
    othpatch --remove nmlgc/aero

A bare patch id works if only one repository has it. Installation gets
the required dependencies; removal also removes dependencies that only
that mod needs. If another patch needs the mod, remove that patch first.

Each normal run also checks for newer releases of the games beside it
and of othpatch, and asks before installing them. To accept available
updates without a question:

    othpatch --update

Development builds are never replaced by release updates. Updates keep
your settings, scores and replays. A game's bundled othpatch replaces
the installed release only when it is newer.

For folder and patch details, or all arguments:

    othpatch --verbose
    othpatch --help

othpatch --dir <folder> selects a custom patches folder. It does not
change the game's patches folder; both must use the same location.


Credits
-------

- ZUN (Team Shanghai Alice): the original game, characters, art and music.
- Cirno Labs: the matching decompilation of the original game on which
  this port is based.
  https://github.com/Cirno-Labs/th07
- thpatch.net community: translations in the patch format of thcrap.
- SDL, SDL_ttf, FreeType, HarfBuzz, FFmpeg, libjpeg-turbo, zlib, curl
  and Mbed TLS: libraries.
- IPAGothic and IPAPGothic: the bundled fonts, IPA fonts by the IPA.

Source and documentation:
https://github.com/Cirno-Labs/opentouhou
Full licensing details available in oth07_licenses.txt.

This is an unofficial port, not endorsed by ZUN.
