# othpatch changelog

Each release lists what changed for players, newest first.

## Unreleased

- Downloads the thcrap patches of thpatch.net for the games beside it:
  the language pack of the system, or the patch named on the command
  line, and updates the patches in the patches folder.
- Checks for a newer release of each game beside it and asks before it
  installs it. `--update` installs it without asking.
- Checks for a newer release of othpatch itself and installs it in the
  same way. A game update never installs an older othpatch than the
  running one.
- Finds the releases in the opentouhou release index. `--releases
  <url>` names another release index.
