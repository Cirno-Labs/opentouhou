# othpatch changelog

## Unreleased

- Changed the text of `--help` and of command-line errors to a new
  format.
- Changed othpatch to find the games beside it and in the installed
  patches, so it handles th07.

## 1.0.0 (2026-11-13)

- Added downloads of the thpatch.net translations for the games beside
  it: the language of the system, or the patch named on the command
  line.
- Added updates for the games beside it. othpatch asks first;
  `--update` installs without asking.
- Added updates for othpatch itself.
- Added `--releases <url>` to read another release index.
- Added support for paths longer than 260 characters on Windows.
