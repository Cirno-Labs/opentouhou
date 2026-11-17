"""Read the game-named readme and combined license document."""

from pathlib import Path


def package_documents(game: str, repo_dir: Path) -> dict[str, bytes]:
    """Keep document filenames the same in the source and each package."""
    folder = repo_dir / "tools/packaging" / game
    return {
        name: (folder / name).read_bytes()
        for name in (f"o{game}_readme.txt", f"o{game}_licenses.txt")
    }
