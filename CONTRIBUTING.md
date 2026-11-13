# Contributing to opentouhou

This guide describes how to set up, test and send a change, and the
rules that every change follows.

## Design

The design is in the documents below. Read the ones that a change
touches before you make it.

- `docs/ARCHITECTURE.md`: platform and tooling, layers, program
  structure, determinism rules, the module map.
- `docs/TH06.md`: how th06 is structured and maps onto the design.
- `docs/BUILDING.md`: building, packaging, checks, tests.

The layer rules and the determinism rules in `docs/ARCHITECTURE.md`
bind every change; simulation code in particular must follow "Simulation
state and determinism" there.

## Setting up

Install the tools that "Requirements" in `docs/BUILDING.md` lists. Then
turn on the checks for your clone, once:

```sh
prek install
```

prek then checks every commit and its message.

## Testing

```sh
just build-linux    # build every game and othpatch
just test           # run the unit tests
just build-win      # cross-compile the Windows executables
prek -a             # run the checks on the whole tree
```

`docs/BUILDING.md` describes the other tests and builds.

## Sending a change

Send your change as a pull request on GitHub against `main`. CI runs the
checks, builds the change for Linux, Windows and macOS, and runs the
unit tests on Linux and macOS. A change that players notice adds an
entry under `## Unreleased` in the changelog of each program that it
affects.

## C style

Naming:

- Variables and struct members: `lower_snake_case`
- Global variables: `g_PascalCase`
- Module variables: `static m_PascalCase`
- Public functions: `Module_PascalCase()`
- Module prefixes are short (`Mem_`, `Str_`, `Array_`, `HashMap_`,
  ...); the name after the underscore is spelled out in full.
- Module functions: `static M_PascalCase()`
- Structs, enums, enum members, macros: `UPPER_SNAKE_CASE`

Rules:

- C23. `nullptr`, never `NULL`. `bool` is built in; do not include
  `<stdbool.h>`.
- Within a file: defines, module types, module variables, module
  functions, public functions.
- Braces always, K&R style. Merge nested `if`s into one condition.
- Compare integers and flags with `== 0` / `!= 0`; test bools as `x` /
  `!x`, never against `true` or `false`.
- `bool` states a fact. A function that can fail returns a result type
  instead, so the signature says which of the two it is.
- No `(void)unused;` statements.
- Comments are `//` only.
- clang-format formats everything; never hand-format against it.

## Comments

Comments are an antipattern by default. Write one only for API
documentation in a public header, or where the code is tricky, and then
in plain words a newcomer would follow. A comment that restates the
code, summarizes a block, or explains ordinary control flow, ordering,
ownership or cleanup is one to delete. A comment describes the code as
it stands, never the change that produced it.

Write comments the way a reference manual is written. Open with a verb
saying what the thing does. Describe states and behaviour, never an
invented actor such as "a reader that ..." or "a caller that ...". Use
the standard technical word whenever one exists: fatal, return, report
failure, error flag, position.

Keep the order linear, so that no sentence has to be read twice. Put the
main clause first and the reason after it. Name the place something
belongs instead of saying "here" or "there", and keep two contrasted
statements in the same shape.

Wrong:

    // Whether reading past the end of the file is the reader's own to
    // answer for. A reader that says nothing stops the game there.

Right:

    // Controls what happens when a read reaches past the end of the file.
    // By default, this is fatal because the plain reads return values and
    // have no way to report failure.

Code taken from a decompilation keeps none of its matching-era comments
(addresses, sizes, "as the original's", padding notes) unless they still
explain something about the code as it now is.

## Documents

Adding to a document is an antipattern by default, as a comment is.
Each document has one job, and a change touches it only when a fact
inside that job changes:

- `docs/ARCHITECTURE.md`: the structure and rules that bind every game.
- `docs/<GAME>.md`: what the original game is like, and the decisions
  that follow from it for more than one module.
- `docs/BUILDING.md`: building, packaging, checks and tests.
- `docs/<program>/CHANGELOG.md`: the changes that players notice.
- `README.md`: what the project is, its status, and where to get it.
- `tools/packaging/<game>/README.txt`: what a player of the package
  needs: installing, the folders, the keys, the settings and othpatch.
- `CONTRIBUTING.md`: how to send a change, and the rules for every
  change.
- `AGENTS.md`: the rules that only agents follow.

What one module does, its quirks and its limits go in its public
header, not in a document. A fact lives in one place: before writing
it, find where it already is, and move or point to it instead of
copying it. A copy goes stale.

Wrong: a section in a game's notes that describes how one module loads
its files.

Right: the header comment of that module, and one line in the game's
notes only if a fact about the original shapes several modules.

Documents describe the current state only: no "previously", no
superseded sections, no change notes. Git history holds the past. When
code and a document disagree, fix the one that is wrong in the same
change. Documents depend in the same direction as code:
`docs/ARCHITECTURE.md` never refers to a game or its notes; a game's
notes may refer to it.

## Prose

For commit messages, documentation and pull requests:

- ASD-STE100 Simplified Technical English: plain words, short sentences,
  one idea each, no flourishes.
- The first sentence says what changed or what the thing is.
- Stay high level: what changed and why it matters, not how it works.
- Give old behaviour only where the reader needs it, in one "Before
  this, ..." sentence.
- To shorten, cut clauses and sentences. Do not compress.

## Changelogs

- A changelog entry says what a player notices, in one sentence that
  starts with `Added`, `Changed`, `Fixed` or `Removed`.
- Never list a fix of a change that no release has shipped. Correct
  the entry of that change instead.

## Commits

- Subject: `module-prefix: imperative description`, at most 50
  characters. The prefix names the module the change touches most,
  spelled as its path: `core/gfx`, `stg/anm`, `th06/chain`, `docs`,
  `tools`. Two modules equally: `core+stg`. Never a change type (`feat`,
  `fix`, `chore`, ...).
- A blank line, then a body wrapped at 72 characters that says what and
  why without repeating the subject.
- No trailers (`Co-Authored-By`, `Signed-off-by`).
- Every commit stands on its own: `prek -a` clean and `just test` green.
- The commit-msg hook (`tools/lint/check_commit_msg`) enforces the
  subject and wrapping rules.
