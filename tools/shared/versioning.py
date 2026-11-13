import os
import re
import subprocess
from pathlib import Path

REPO_DIR = Path(__file__).resolve().parents[2]
VERSION = re.compile(r"\d+\.\d+\.\d+")
# Names the environment variable that makes a build a release build.
RELEASE_ENV = "OPENTOUHOU_RELEASE"


def version_path(program: str, repo_dir: Path = REPO_DIR) -> Path:
    """Return the path of a program's version file, src/<program>/version.txt."""
    return repo_dir / "src" / program / "version.txt"


def othpatch_pin_path(game: str, repo_dir: Path = REPO_DIR) -> Path:
    """Return the path of the file that names the othpatch version of a
    game's packages, src/<game>/othpatch.txt."""
    return repo_dir / "src" / game / "othpatch.txt"


def read_version(path: Path) -> str:
    """Return the version in a one-line file, such as 1.2.0.

    Raises ValueError when the line is not three numbers separated by
    dots.
    """
    text = path.read_text().strip()
    if not VERSION.fullmatch(text):
        raise ValueError(f"{path} holds {text!r} instead of a version X.Y.Z")
    return text


def programs(repo_dir: Path = REPO_DIR) -> list[str]:
    """Return the programs that have a version file, in name order."""
    return sorted(path.parent.name for path in (repo_dir / "src").glob("*/version.txt"))


def games(repo_dir: Path = REPO_DIR) -> list[str]:
    """Return the games: the programs other than othpatch, in name order."""
    return [program for program in programs(repo_dir) if program != "othpatch"]


def generate_version(program: str, repo_dir: Path = REPO_DIR) -> str:
    """Return the version that a build of a program reports.

    A release build, with the environment variable OPENTOUHOU_RELEASE set
    to 1, reports the version of src/<program>/version.txt, such as 1.2.0.
    Every other build adds -dev and the short commit, as in
    1.2.0-dev+abcdef0, or -dev alone when git cannot name the commit.
    """
    version = read_version(version_path(program, repo_dir))
    if os.environ.get(RELEASE_ENV) == "1":
        return version
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short=7", "HEAD"],
            cwd=repo_dir,
            capture_output=True,
            text=True,
        )
    except OSError:
        return f"{version}-dev"
    commit = result.stdout.strip()
    if result.returncode != 0 or not commit:
        return f"{version}-dev"
    return f"{version}-dev+{commit}"


def product_name(program: str) -> str:
    """Return the name that a program is published under: othpatch, or
    for a game the game id with an o before it, such as oth06.

    The program's executable and its files carry this name. A game's
    executable sits in the original game's folder by default, so its name
    must differ from the original's, such as th06.exe.
    """
    return program if program == "othpatch" else f"o{program}"


def file_version(version: str) -> str:
    """Return a version as a file name can hold it.

    A + in a URL can stand for a space, so a development version such as
    1.2.0-dev+abcdef0 becomes 1.2.0-dev-abcdef0.
    """
    return version.replace("+", "-")


PLATFORM_NAMES = {"linux": "Linux", "win": "Windows", "mac": "macOS"}


def generate_package_name(game: str, version: str, platform: str) -> str:
    """Return the name of a game's package without .zip, such as
    oth06-1.2.0-Linux."""
    return f"{product_name(game)}-{file_version(version)}-{PLATFORM_NAMES[platform]}"


def othpatch_asset_name(version: str, platform: str) -> str:
    """Return the name of othpatch's executable in its release, such as
    othpatch-1.2.0-Windows.exe."""
    suffix = ".exe" if platform == "win" else ""
    return f"othpatch-{file_version(version)}-{PLATFORM_NAMES[platform]}{suffix}"
