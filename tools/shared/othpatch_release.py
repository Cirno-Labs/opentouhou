import hashlib
import json
import os
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

from shared.versioning import (
    PLATFORM_NAMES,
    REPO_DIR,
    othpatch_asset_name,
    othpatch_pin_path,
    read_version,
)


def releases_url(repo_dir: Path = REPO_DIR) -> str:
    """Return the URL of the release index that packaging reads.

    It is the environment variable RELEASE_INDEX_URL, else the build
    option releases_url, as the environment variable RELEASES_URL or the
    default in meson.options gives it. RELEASE_INDEX_URL changes the index
    of packaging alone, not the one that the built othpatch reads.
    """
    for name in ["RELEASE_INDEX_URL", "RELEASES_URL"]:
        if os.environ.get(name):
            return os.environ[name]
    options = (repo_dir / "meson.options").read_text()
    match = re.search(r"'releases_url',.*?value: '([^']*)'", options, re.DOTALL)
    if match is None:
        raise RuntimeError("meson.options has no default for releases_url")
    return match.group(1)


# Names the client in requests. The public address of an R2 bucket
# refuses Python's default User-Agent with HTTP 403.
USER_AGENT = "opentouhou-packaging"


def read_url(url: str, what: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(request) as response:
            return response.read()
    except urllib.error.HTTPError as error:
        raise RuntimeError(f"Cannot read {what} at {url}: {error}") from None
    except urllib.error.URLError as error:
        raise RuntimeError(
            f"Cannot read {what} at {url}: {error.reason}."
            " Packaging downloads othpatch and needs the network."
        ) from None


def fetch_othpatch(version: str, platform: str, repo_dir: Path = REPO_DIR) -> Path:
    """Return the path of othpatch's executable for a platform, downloaded
    into build/othpatch/ as the release index gives it.

    The file is checked against the SHA-256 digest that the index gives.
    Raises RuntimeError with a message for a person when the release or
    the file is missing or the digest is wrong.
    """
    url = releases_url(repo_dir)
    try:
        index = json.loads(read_url(url, "the release index"))
    except ValueError:
        raise RuntimeError(f"The release index at {url} is not JSON") from None
    if not isinstance(index, dict) or not isinstance(index.get("programs"), dict):
        raise RuntimeError(f"{url} is not a release index")
    release = next(
        (
            item
            for item in index["programs"].get("othpatch", [])
            if item.get("version") == version
        ),
        None,
    )
    if release is None:
        raise RuntimeError(
            f"The release index at {url} has no othpatch {version}. Publish"
            " it first, or name a released version in src/<game>/othpatch.txt."
        )
    name = othpatch_asset_name(version, platform)
    file = release.get("files", {}).get(PLATFORM_NAMES[platform])
    if file is None or file.get("name") != name:
        raise RuntimeError(f"othpatch {version} in the index at {url} has no {name}")
    digest = str(file.get("sha256", "")).lower()
    if not re.fullmatch(r"[0-9a-f]{64}", digest):
        raise RuntimeError(f"The index at {url} gives no SHA-256 digest of {name}")

    path = repo_dir / "build/othpatch" / name
    if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest() == digest:
        return path
    data = read_url(file["url"], name)
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
