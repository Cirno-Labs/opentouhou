"""Build the Discord message that announces released programs.

The message holds one embed per program: its product name and version,
its changelog section, a link to its page on the site and its icon.
Discord limits a message's embeds to 6000 characters in all, so the
longest changelogs lose their last entries first, with a note that the
rest is on the site.
"""

from shared.versioning import product_name

SITE_URL = "https://opentouhou.org"
# Discord's limits for a description and for all the embeds of a message,
# less a margin for the titles.
DESCRIPTION_LIMIT = 4096
MESSAGE_LIMIT = 5800
MORE = "…and more on the site."


def entries(changes: str) -> list[str]:
    """Return the entries of a changelog section, each on one line.

    A summary paragraph before the list counts as one entry.
    """
    found: list[str] = []
    for line in changes.splitlines():
        if not line.strip():
            if found and found[-1]:
                found.append("")
            continue
        if line.startswith("- ") or not found or not found[-1]:
            found.append(line.strip())
        else:
            found[-1] += " " + line.strip()
    return [entry for entry in found if entry]


def fit(lines: list[str], limit: int) -> str:
    """Join lines, leaving out the last ones and noting it when they pass
    limit characters."""
    text = "\n".join(lines)
    if len(text) <= limit:
        return text
    kept: list[str] = []
    for line in lines:
        if len("\n".join([*kept, line, MORE])) > limit:
            break
        kept.append(line)
    return "\n".join([*kept, MORE])


def embed(program: str, version: str, changes: str, limit: int) -> dict:
    """Return the embed of one program's release."""
    name = product_name(program)
    return {
        "title": f"{name} {version}",
        "url": f"{SITE_URL}/{name}-releases#v{version}",
        "description": fit(entries(changes), min(limit, DESCRIPTION_LIMIT)),
        "thumbnail": {"url": f"{SITE_URL}/icons/{name}.webp"},
    }


def message(releases: list[tuple[str, str, str]]) -> dict:
    """Return the message for releases, each (program, version, changes).

    The shorter changelogs are sized first, so that only the longest
    ones are cut to share MESSAGE_LIMIT.
    """
    order = sorted(range(len(releases)), key=lambda i: len(releases[i][2]))
    embeds: dict[int, dict] = {}
    left = MESSAGE_LIMIT
    for done, i in enumerate(order):
        program, version, changes = releases[i]
        item = embed(program, version, changes, left // (len(order) - done))
        left -= len(item["title"]) + len(item["description"])
        embeds[i] = item
    return {"embeds": [embeds[i] for i in range(len(releases))]}
