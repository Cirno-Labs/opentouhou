# th06 changelog

## Unreleased

- Added loading of font files that you put in `opentouhou/fonts/` of
  the game folder, in place of fonts of the same name.
- Changed the program icon to a redrawn Remilia with an outline that
  stays visible on dark backgrounds.
- Changed package documents to `oth06_readme.txt` and
  `oth06_licenses.txt` so they do not replace the original game's readme.
- Changed the conversion of music and sound effects to the output rate,
  which keeps their high frequencies.
- Changed the text font to MS Gothic, as in the original, when the
  system has it, and to the bundled IPAGothic otherwise, instead
  of BIZ UDGothic.
- Changed the text of `--help` and of command-line errors to a new
  format.
- Fixed the missing frame rate display at the bottom of the screen, as
  in the original, outside ending scenes; `[game] show_fps` hides it.
- Fixed a gamepad or joystick button bound to both shoot and focus: it
  now focuses only after it is held for 8 frames, as in the original.
- Fixed some textures looking darker than in the original.
- Fixed transparency in translated textures.
- Fixed JPEG background colors and shading.
- Fixed a black screen showing instead of the startup logo.
- Fixed the timing of the startup logo's fade into the title screen.
- Fixed how stage backgrounds fade into fog with distance.
- Fixed faint, almost transparent sprite edges that the original does
  not draw.
- Fixed texture distortion and color seams across sprites and
  backgrounds, which split along the wrong diagonal.
- Fixed overlapping sprites drawing in the wrong order, and items, the
  player, bombs and spell card backgrounds showing outside the
  playfield.
- Fixed background fades starting from black instead of from the
  previous frame.
- Fixed sprites, the power bar, the dialogue box and translator notes
  sitting half a pixel off.
- Fixed enemies flashing white when player shots hit them, and sprites
  sampled without the original's smoothing.
- Fixed orbiting boss effects in dark, saturated colors instead of the
  original's pale tints.
- Fixed the spacing of bold translated text.
- Fixed the shading of text in spell names, dialogue and stage titles.
- Fixed the placement of translated stage and song titles.
- Fixed the shape of translator notes, and their weight in fonts that
  patches bring.
- Fixed the alignment of translated stage labels, bonus notices and
  spell card bonus labels.
- Fixed the colors of highlighted entries in the title, options and
  pad button menus.
- Fixed the options menu showing no color depth as chosen: it now shows
  32-bit color.
- Fixed the right half of translated Music Room comments.
- Fixed score widths and the untranslated clear label on translated
  results screens.
- Fixed the brightness of entries in the shot type select and the
  results menu.
- Fixed the selected rows of the replay menu, which showed in pink
  instead of the original's white selection font.
- Fixed dark lines along the edges of sprites, such as through the
  player select portraits, with smooth scaling and interpolation.

## 1.0.1 (2026-11-15)

- Fixed the message about missing game data on macOS when macOS starts
  a temporary copy of the program: it now tells how to stop that.

## 1.0.0 (2026-11-13)

- Added th06, compatible with the original down to its replays: the 17
  world record replays play back frame for frame.
- Added native packages for Windows, Linux and macOS.
- Added English and other languages through othpatch.
- Added smooth motion between ticks on fast displays.
- Added `[video] scaling` in config.ini: sharp, smooth or integer
  scaling of the screen to the window.
- Added F11 and Alt+Enter to switch between window and fullscreen.
- Added rebindable keys and gamepad buttons.
- Added portable mode with a `portable.txt` beside the game.
- Added `--user-dir` to keep your files in another folder.
- Added a message box when the game cannot start or stops on an error.
- Added a log of the last start in `oth06.log` in the user folder.
- Added support for paths longer than 260 characters on Windows.
