import subprocess
from pathlib import Path

REPO_DIR = Path(__file__).resolve().parents[2]


def generate_version(game: str, repo_dir: Path = REPO_DIR) -> str:
    """Return a game's version: its latest `<game>-v*` tag, else the short
    commit.

    A tag adds the commits since it and the commit, as in
    `1.0.0-3-gabcdef0`. A tree with uncommitted changes adds `-dirty`.
    Each game has its own version; core has none.
    """
    result = subprocess.run(
        [
            "git",
            "describe",
            "--tags",
            "--match",
            f"{game}-v*",
            "--always",
            "--abbrev=7",
            "--dirty",
        ],
        cwd=repo_dir,
        capture_output=True,
        text=True,
    )
    version = result.stdout.strip()
    if result.returncode != 0 or not version:
        return "unknown"
    return version.removeprefix(f"{game}-v")


def executable_name(game: str) -> str:
    """Return the name of a game's executable: the game id with an o
    before it, such as oth06.

    The executable sits in the original game's folder by default, so its
    name must differ from the original's, such as th06.exe.
    """
    return f"o{game}"


def generate_package_name(game: str, version: str, platform: str) -> str:
    platform_name = {"linux": "Linux", "win": "Windows", "mac": "macOS"}[platform]
    return f"{executable_name(game)}-{version}-{platform_name}"
