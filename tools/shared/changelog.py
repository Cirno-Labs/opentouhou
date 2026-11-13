# SPDX-License-Identifier: GPL-3.0-only
"""Read the changelogs, docs/<program>/CHANGELOG.md.

A changelog has this format:

    # <program> changelog

    ## Unreleased

    - Added ...
    - Fixed ...

    ## 1.2.0 (2026-11-11)

    One optional paragraph that sums up the release.

    - Added ...
      a wrapped line, indented by two spaces.
    - Changed ...

The first line names the program, and no text follows it before the
first section. "## Unreleased" comes first and is present only when it
has entries. The released versions follow, newest first, each with the
date of its release. A section holds an optional summary paragraph,
then a list of entries. Each entry starts with one of VERBS, and the
entries are grouped in the order of VERBS.
"""

import dataclasses
import datetime
import re
from pathlib import Path

REPO_DIR = Path(__file__).resolve().parents[2]
UNRELEASED = "Unreleased"
VERBS = ["Added", "Changed", "Fixed", "Removed"]
HEADING = re.compile(r"## (\d+\.\d+\.\d+) \((\d{4}-\d{2}-\d{2})\)")


@dataclasses.dataclass
class Section:
    """One section of a changelog: its heading line, its version, or
    Unreleased, its date, or None, and its lines after the heading."""

    heading: str
    title: str
    date: str | None
    lines: list[str]

    def text(self) -> str:
        """Return the heading and the body, without blank lines at the
        end."""
        lines = [self.heading, *self.lines]
        while lines and not lines[-1].strip():
            lines.pop()
        return "\n".join(lines) + "\n"

    def body(self) -> str:
        """Return the body without its heading and without blank lines
        around it."""
        return "\n".join(self.lines).strip()


def changelog_path(program: str, repo_dir: Path = REPO_DIR) -> Path:
    """Return the path of a program's changelog, docs/<program>/CHANGELOG.md."""
    return repo_dir / "docs" / program / "CHANGELOG.md"


def heading(version: str, date: str) -> str:
    """Return the heading of a released version, such as
    ## 1.2.0 (2026-11-11)."""
    return f"## {version} ({date})"


def sections(text: str) -> list[Section]:
    """Return the sections of a changelog in file order.

    A section starts at a line that starts with "## ". The title is the
    version of a released heading, Unreleased for "## Unreleased", and
    the whole heading text for any other heading.
    """
    found = []
    for line in text.splitlines():
        if line.startswith("## "):
            match = HEADING.fullmatch(line)
            if match:
                found.append(Section(line, match.group(1), match.group(2), []))
            else:
                found.append(Section(line, line[3:], None, []))
        elif found:
            found[-1].lines.append(line)
    return found


def find(text: str, title: str) -> Section | None:
    """Return the section of a version, or of Unreleased, or None."""
    return next((item for item in sections(text) if item.title == title), None)


def section(text: str, title: str) -> str | None:
    """Return the body of the section of a version, or of Unreleased,
    or None when the changelog has no such section."""
    found = find(text, title)
    return found.body() if found is not None else None


def date(text: str, version: str) -> str | None:
    """Return the date in the heading of a released version, or None
    when the changelog has no such heading."""
    found = find(text, version)
    return found.date if found is not None else None


def version_key(version: str) -> tuple[int, ...]:
    return tuple(int(part) for part in version.split("."))


def check_body(item: Section) -> list[str]:
    errors = []
    lines = list(item.lines)
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    if lines and not lines[0].startswith("- "):
        end = next((i for i, line in enumerate(lines) if not line.strip()), len(lines))
        if end == len(lines):
            return [f"{item.heading}: the summary has no list of entries after it"]
        lines = lines[end:]
        while lines and not lines[0].strip():
            lines.pop(0)
        if lines and not lines[0].startswith("- "):
            errors.append(f"{item.heading}: the summary must be one paragraph")
    if not lines and item.title == UNRELEASED:
        return errors + [
            "## Unreleased has no entries; remove the heading until it has one"
        ]
    if not lines:
        return errors + [f"{item.heading}: the section has no entries"]
    order = -1
    for line in lines:
        if line.startswith("- "):
            verb = line[2:].split(" ", 1)[0]
            if verb not in VERBS:
                errors.append(
                    f"{item.heading}: the entry '{line}' must start with"
                    f" {', '.join(VERBS[:-1])} or {VERBS[-1]}"
                )
                continue
            if VERBS.index(verb) < order:
                errors.append(
                    f"{item.heading}: the entry '{line}' is out of order;"
                    f" the entries go in the order {', '.join(VERBS)}"
                )
            order = max(order, VERBS.index(verb))
        elif not line.strip():
            errors.append(f"{item.heading}: the list of entries has a blank line")
        elif not line.startswith("  ") or line.startswith("   "):
            errors.append(
                f"{item.heading}: the line '{line}' must start with '- ' or,"
                " as a wrapped line of an entry, with two spaces"
            )
    return errors


def check(text: str, program: str) -> list[str]:
    """Return the errors in the format of a program's changelog, or an
    empty list."""
    lines = text.splitlines()
    title = f"# {program} changelog"
    if not lines or lines[0] != title:
        return [f"the first line must be '{title}'"]
    errors = []
    first = next(
        (i for i, line in enumerate(lines) if line.startswith("## ")), len(lines)
    )
    if any(line.strip() for line in lines[1:first]):
        errors.append(
            "the title must be followed by the first section, without other text"
        )
    errors += [
        f"the heading '{line}' is not allowed; use only '## ' headings and"
        " list the entries without subsections"
        for line in lines
        if line.startswith("#") and not line.startswith("## ") and line != title
    ]
    previous = None
    for i, item in enumerate(sections(text)):
        if item.title == UNRELEASED and item.heading == f"## {UNRELEASED}":
            if i != 0:
                errors.append("## Unreleased must be the first section")
        elif item.date is None:
            errors.append(
                f"the heading '{item.heading}' must be '## Unreleased' or"
                " '## X.Y.Z (YYYY-MM-DD)'"
            )
            continue
        else:
            try:
                datetime.date.fromisoformat(item.date)
            except ValueError:
                errors.append(f"{item.heading}: {item.date} is not a date")
            if previous is not None and version_key(item.title) >= version_key(
                previous
            ):
                errors.append(
                    f"{item.heading}: the versions must go newest first,"
                    f" and {item.title} follows {previous}"
                )
            previous = item.title
        errors += check_body(item)
    return errors
