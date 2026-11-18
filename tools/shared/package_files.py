"""Read optional per-game supplemental files for all package formats."""

import json
import shutil
from pathlib import Path, PurePosixPath


def _relative_path(value: object) -> str:
    if not isinstance(value, str) or not value or "\\" in value or ":" in value:
        raise ValueError(f"Invalid package path: {value!r}")
    if any(ord(char) < 32 or ord(char) == 127 or char in '<>"|?*' for char in value):
        raise ValueError(f"Invalid package path: {value!r}")
    parts = value.split("/")
    reserved = {"con", "prn", "aux", "nul"} | {
        f"{prefix}{number}" for prefix in ("com", "lpt") for number in range(1, 10)
    }
    if any(
        part.endswith((".", " ")) or part.split(".")[0].casefold() in reserved
        for part in parts
    ):
        raise ValueError(f"Invalid package path: {value!r}")
    path = PurePosixPath(value)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in parts):
        raise ValueError(f"Invalid package path: {value!r}")
    return value


def package_files(game: str, repo_dir: Path) -> list[tuple[Path, str]]:
    """Validate repo-relative sources and package-relative destinations.

    A missing files.json means no supplemental files. Each manifest row
    is [source, destination]. Reserve the standard package entries.
    """
    manifest = repo_dir / "tools/packaging" / game / "files.json"
    if not manifest.exists():
        return []
    rows = json.loads(manifest.read_text())
    if not isinstance(rows, list):
        raise ValueError(f"Expected a list in {manifest}")
    files: list[tuple[Path, str]] = []
    names = {
        "readme.txt",
        f"o{game}_readme.txt",
        f"o{game}_licenses.txt",
        "othpatch",
        "othpatch.exe",
        f"o{game}.exe",
        f"o{game}.appimage",
        f"{game}.icns",
    }
    for row in rows:
        if not isinstance(row, list) or len(row) != 2:
            raise ValueError(f"Expected [source, destination] in {manifest}")
        source = repo_dir / _relative_path(row[0])
        name = _relative_path(row[1])
        key = name.casefold()
        if any(
            key == old or key.startswith(old + "/") or old.startswith(key + "/")
            for old in names
        ):
            raise ValueError(f"Conflicting package destination: {name}")
        if not source.resolve().is_relative_to(repo_dir.resolve()):
            raise ValueError(f"Package source leaves repository: {source}")
        if not source.is_file():
            raise FileNotFoundError(source)
        names.add(key)
        files.append((source, name))
    return files


def copy_package_files(files: list[tuple[Path, str]], destination: Path) -> None:
    """Copy validated supplemental files under the package document root."""
    for source, name in files:
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
