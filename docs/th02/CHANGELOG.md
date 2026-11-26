# th02 changelog

## Unreleased

- Added a default-on fix that omits the missing final bomb transition
  mask; disabling `[fixes] bomb_transition_overread` draws the memory that
  the original reads in its place.
