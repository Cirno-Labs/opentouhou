import re
from pathlib import Path

REPO_DIR = Path(__file__).resolve().parents[2]


def changelog_path(game: str, repo_dir: Path = REPO_DIR) -> Path:
    """Return the path of a game's changelog, docs/<game>/CHANGELOG.md."""
    return repo_dir / "docs" / game / "CHANGELOG.md"


def games_with_changelogs(repo_dir: Path = REPO_DIR) -> list[str]:
    """Return the games that have a changelog, in name order."""
    return sorted(path.parent.name for path in (repo_dir / "docs").glob("*/CHANGELOG.md"))


def section(text: str, title: str) -> str | None:
    """Return the body of the `## <title>` section of a changelog, without
    its heading, or None when there is no such section.

    A heading matches when it is exactly the title or the title followed by
    a space, as in `## 1.2.0 - 2026-11-11`.
    """
    lines = text.splitlines()
    for i, line in enumerate(lines):
        if line == f"## {title}" or line.startswith(f"## {title} "):
            body = []
            for other in lines[i + 1 :]:
                if other.startswith("## "):
                    break
                body.append(other)
            return "\n".join(body).strip()
    return None


def date(text: str, title: str) -> str | None:
    """Return the date of the `## <title> - YYYY-MM-DD` heading of a
    changelog, or None when the section has no such heading."""
    match = re.search(
        rf"^## {re.escape(title)} - (\d{{4}}-\d{{2}}-\d{{2}})$", text, re.MULTILINE
    )
    return match.group(1) if match else None
