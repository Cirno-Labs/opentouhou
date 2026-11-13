# Working on opentouhou

Most code here is written by agents. Every rule for a change is in
[CONTRIBUTING.md](CONTRIBUTING.md): read it first, and the design
documents that it lists. This file adds only what is specific to
agents.

# Checking your work

- Run `prek -a` and `just test` on each commit of a series, not only on
  the last.

# Safety

- The original game data is the user's, never in git, and has no backup.
  Read it; never write, move, delete or copy it, and never commit any of
  it or anything derived from it (images, sounds, scripts, traces).
- No destructive git commands (`reset --hard`, `checkout --`, `clean`,
  `push --force`, ...) unless a person asks for them in the
  conversation.
- Never revert or reorder a person's edits unless they ask.
