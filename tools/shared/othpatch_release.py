import hashlib
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from collections.abc import Callable
from pathlib import Path

from shared.versioning import (
    PLATFORM_NAMES,
    RELEASE_ENV,
    REPO_DIR,
    othpatch_asset_name,
    othpatch_pin_path,
    read_version,
)


def build_releases_url(repo_dir: Path = REPO_DIR) -> str:
    """Return the value of the build option releases_url: the environment
    variable RELEASES_URL, else the default in meson.options."""
    if os.environ.get("RELEASES_URL"):
        return os.environ["RELEASES_URL"]
    options = (repo_dir / "meson.options").read_text()
    match = re.search(r"'releases_url',.*?value: '([^']*)'", options, re.DOTALL)
    if match is None:
        raise RuntimeError("meson.options has no default for releases_url")
    return match.group(1)


def releases_url(repo_dir: Path = REPO_DIR) -> str:
    """Return the URL of the release index that packaging reads.

    It is the environment variable RELEASE_INDEX_URL, else the build
    option releases_url from build_releases_url. RELEASE_INDEX_URL
    changes the index of packaging alone, not the one that the built
    othpatch reads.
    """
    if os.environ.get("RELEASE_INDEX_URL"):
        return os.environ["RELEASE_INDEX_URL"]
    return build_releases_url(repo_dir)


# Names the client in requests. The public address of an R2 bucket
# refuses Python's default User-Agent with HTTP 403.
USER_AGENT = "opentouhou-packaging"


def request(url: str) -> bytes:
    """Return the body of the response to a GET request of url.

    Raises the errors of urllib.request.urlopen.
    """
    with urllib.request.urlopen(
        urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    ) as response:
        return response.read()


def read_url(url: str, what: str) -> bytes:
    try:
        return request(url)
    except urllib.error.HTTPError as error:
        raise RuntimeError(f"Cannot read {what} at {url}: {error}") from None
    except urllib.error.URLError as error:
        raise RuntimeError(
            f"Cannot read {what} at {url}: {error.reason}."
            " Packaging downloads othpatch and needs the network."
        ) from None


class IndexUnavailable(RuntimeError):
    """Reports that the release index cannot be reached at all, as
    opposed to an index that is reached and is wrong."""


def read_index(url: str) -> dict:
    """Return the release index at url. An index that does not exist yet
    (HTTP 404) has no releases.

    Raises IndexUnavailable when the host of url ends in .invalid, as
    the default of the build option releases_url does, or when the
    request fails. Raises RuntimeError when the index is not valid.
    """
    host = urllib.parse.urlsplit(url).hostname or ""
    if host.endswith(".invalid"):
        raise IndexUnavailable(f"The release index at {url} is a placeholder")
    try:
        data = request(url)
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return {"format": 1, "programs": {}}
        raise IndexUnavailable(
            f"Cannot read the release index at {url}: {error}"
        ) from None
    except urllib.error.URLError as error:
        raise IndexUnavailable(
            f"Cannot read the release index at {url}: {error.reason}"
        ) from None
    except (ValueError, OSError) as error:
        raise IndexUnavailable(
            f"Cannot read the release index at {url}: {error}"
        ) from None
    try:
        index = json.loads(data)
    except ValueError:
        raise RuntimeError(f"The release index at {url} is not JSON") from None
    if not isinstance(index, dict) or not isinstance(index.get("programs"), dict):
        raise RuntimeError(f"{url} is not a release index")
    return index


def fetch_othpatch(
    version: str, platform: str, repo_dir: Path = REPO_DIR
) -> Path | None:
    """Return the path of othpatch's executable for a platform, downloaded
    into build/othpatch/ as the release index gives it, or None when the
    index has no othpatch of that version.

    The file is checked against the SHA-256 digest that the index gives.
    Raises IndexUnavailable when the index cannot be reached, and
    RuntimeError with a message for a person when the index is not
    valid, the file is missing or the digest is wrong.
    """
    url = releases_url(repo_dir)
    index = read_index(url)
    release = next(
        (
            item
            for item in index["programs"].get("othpatch", [])
            if item.get("version") == version
        ),
        None,
    )
    if release is None:
        return None
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


def pinned_othpatch(
    game: str, platform: str, build: Callable[[], Path], repo_dir: Path = REPO_DIR
) -> Path:
    """Return the path of the othpatch that a game's packages ship.

    It is the release of the version in src/<game>/othpatch.txt, from
    fetch_othpatch. When the index has no such release, or outside a
    release build cannot be reached, it is the othpatch built from the
    same commit, which build builds and returns. So packaging works
    before the first othpatch release and without the network. A
    release build (OPENTOUHOU_RELEASE=1) ships released othpatch only.
    Prints which othpatch the packages carry, and exits with the
    message of a failure.
    """
    release = bool(os.environ.get(RELEASE_ENV))
    try:
        version = read_version(othpatch_pin_path(game, repo_dir))
        path = fetch_othpatch(version, platform, repo_dir)
    except IndexUnavailable as error:
        if release:
            sys.exit(f"{error}. A release build needs the index.")
        path = build()
        print(
            f"Warning: {error}, so the {game} packages carry the othpatch"
            " of this commit."
        )
        return path
    except (RuntimeError, ValueError) as error:
        sys.exit(str(error))
    if path is not None:
        print(f"The {game} packages carry othpatch {version} from its release.")
        return path
    if release:
        sys.exit(
            f"The release index at {releases_url(repo_dir)} has no othpatch"
            f" {version}. A release ships a released othpatch: publish it"
            " first, or name a released version in"
            f" src/{game}/othpatch.txt."
        )
    path = build()
    print(
        f"The {game} packages carry the othpatch of this commit, because"
        f" othpatch {version} has no release."
    )
    return path
