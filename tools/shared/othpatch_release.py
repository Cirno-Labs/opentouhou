import hashlib
import json
import os
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

from shared.versioning import (
    REPO_DIR,
    othpatch_asset_name,
    othpatch_pin_path,
    read_version,
)


def othpatch_repository(repo_dir: Path = REPO_DIR) -> str:
    """Return the GitHub repository that publishes othpatch's releases.

    It is the build option othpatch_repository, else game_repository, as
    the environment variables OTHPATCH_REPOSITORY and GAME_REPOSITORY or
    the defaults in meson.options give them.
    """
    for name in ["OTHPATCH_REPOSITORY", "GAME_REPOSITORY"]:
        if os.environ.get(name):
            return os.environ[name]
    options = (repo_dir / "meson.options").read_text()
    match = re.search(r"'game_repository',.*?value: '([^']*)'", options, re.DOTALL)
    if match is None:
        raise RuntimeError("meson.options has no default for game_repository")
    return match.group(1)


def request(url: str, accept: str) -> urllib.request.Request:
    """Return a request to the GitHub API, with the token of GH_TOKEN or
    GITHUB_TOKEN when one is set.

    The token is never sent on to a redirect, because the download URL
    of a file redirects to storage that refuses a second credential.
    """
    req = urllib.request.Request(url, headers={"Accept": accept})
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_unredirected_header("Authorization", f"Bearer {token}")
    return req


def fetch_othpatch(version: str, platform: str, repo_dir: Path = REPO_DIR) -> Path:
    """Return the path of othpatch's executable for a platform from its
    release othpatch-v<version>, downloaded into build/othpatch/.

    The file is checked against the SHA-256 digest that the release gives.
    Raises RuntimeError with a message for a person when the release or
    the file is missing or the digest is wrong.
    """
    repository = othpatch_repository(repo_dir)
    tag = f"othpatch-v{version}"
    name = othpatch_asset_name(version, platform)
    url = f"https://api.github.com/repos/{repository}/releases/tags/{tag}"
    try:
        with urllib.request.urlopen(
            request(url, "application/vnd.github+json")
        ) as response:
            release = json.load(response)
    except urllib.error.HTTPError as error:
        if error.code == 404:
            raise RuntimeError(
                f"{repository} has no release {tag}. Publish othpatch {version}"
                " first, or name a released version in src/<game>/othpatch.txt."
            ) from None
        raise RuntimeError(
            f"Cannot read the release {tag} of {repository}: {error}"
        ) from None
    except urllib.error.URLError as error:
        raise RuntimeError(
            f"Cannot read the release {tag} of {repository}: {error.reason}."
            " Packaging downloads othpatch and needs the network."
        ) from None

    asset = next(
        (item for item in release.get("assets", []) if item["name"] == name), None
    )
    if asset is None:
        raise RuntimeError(f"The release {tag} of {repository} has no {name}")
    digest = (asset.get("digest") or "").removeprefix("sha256:").lower()
    if not re.fullmatch(r"[0-9a-f]{64}", digest):
        raise RuntimeError(
            f"The release {tag} of {repository} gives no SHA-256 digest of {name}"
        )

    path = repo_dir / "build/othpatch" / name
    if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest() == digest:
        return path
    with urllib.request.urlopen(
        request(asset["url"], "application/octet-stream")
    ) as response:
        data = response.read()
    computed = hashlib.sha256(data).hexdigest()
    if computed != digest:
        raise RuntimeError(
            f"{name} has the SHA-256 digest {computed} instead of {digest}"
        )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    path.chmod(0o755)
    return path


def pinned_othpatch(game: str, platform: str, repo_dir: Path = REPO_DIR) -> Path:
    """Return the path of the othpatch that a game's packages ship: the
    version of src/<game>/othpatch.txt, from fetch_othpatch.

    Exits with the message of a failure.
    """
    try:
        return fetch_othpatch(
            read_version(othpatch_pin_path(game, repo_dir)), platform, repo_dir
        )
    except (RuntimeError, ValueError) as error:
        sys.exit(str(error))
