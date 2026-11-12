"""Read and write releases.json, the index of the published files.

The index lives at the root of the R2 bucket, beside the files it lists.
othpatch reads it to find updates, and packaging reads it to find the
othpatch that a game ships. It names each program by its product name,
shared.versioning.product_name, such as oth06 or othpatch. The functions
here take the program as the source tree names it, such as th06. The
index is one JSON object:

    {
      "format": 1,
      "programs": {
        "othpatch": [
          {
            "version": "0.1.2",
            "date": "2026-11-12",
            "published": "2026-11-12T14:03:00Z",
            "changes": "- Fixed ...",
            "files": {
              "Linux": {
                "name": "othpatch-0.1.2-Linux",
                "url": "https://.../othpatch/0.1.2/othpatch-0.1.2-Linux",
                "size": 1234567,
                "sha256": "<64 hexadecimal digits>",
                "sha1": "<40 hexadecimal digits>"
              },
              "Windows": {...},
              "macOS": {...}
            },
            "checksums": {
              "SHA256SUMS": "https://.../othpatch/0.1.2/SHA256SUMS",
              "SHA1SUMS": "https://.../othpatch/0.1.2/SHA1SUMS"
            }
          }
        ],
        "oth06": [{"version": "0.1.1", "othpatch": "0.1.2", ...}]
      },
      "snapshot": {
        "commit": "<full commit hash>",
        "date": "2026-11-12T14:03:00Z",
        "programs": {
          "oth06": {"version": "0.1.2-dev-abcdef0", "changes": "...",
                    "files": {...}}
        },
        "checksums": {...}
      }
    }

"programs" maps each program to its releases, newest first. "date" is
the date of the version's changelog heading. "published" is the UTC
time of the version's first upload; a release published before the
field existed has none. "changes" is the body of
that changelog section, as Markdown. "files" maps a platform (Linux,
Windows, macOS) to its file; a platform without a file is left out.
"size" (in bytes), "sha256" and "sha1" are computed from the uploaded
file. "checksums" gives the URLs of the SHA256SUMS and SHA1SUMS files
beside the files, in the format of sha256sum and sha1sum, so that
`sha256sum -c SHA256SUMS` checks the downloads in one folder. A game's
release also gives "othpatch", the othpatch version its packages ship.
"snapshot" is the development snapshot, or is absent: the commit, the
UTC time of the upload, and per program its development version, its
unreleased changes and its files, and the checksum files of all its
files.

A release's files, with its SHA256SUMS and SHA1SUMS, are at
<product>/<version>/<name> in the bucket, such as oth06/0.1.1/, and a
snapshot's at snapshot/<name>. The git tag of a release is
<product>-v<version>, such as oth06-v0.1.1. A version in the index changes only when it is
replaced, less than REPLACE_WINDOW after its "published" time. The
replacement keeps that time, so that the window does not grow.
"""

import datetime
import hashlib
import json
from pathlib import Path

from shared.versioning import file_version, product_name

FORMAT = 1
PLATFORMS = ["Linux", "Windows", "macOS"]
CHECKSUMS = {"SHA256SUMS": "sha256", "SHA1SUMS": "sha1"}
REPLACE_WINDOW = datetime.timedelta(hours=24)


def empty() -> dict:
    """Return an index without releases."""
    return {"format": FORMAT, "programs": {}}


def load(path: Path) -> dict:
    """Return the index in a file.

    Raises ValueError when the file is not an index of this format.
    """
    index = json.loads(path.read_text())
    if not isinstance(index, dict) or index.get("format") != FORMAT:
        raise ValueError(f"{path} is not a release index of format {FORMAT}")
    index.setdefault("programs", {})
    return index


def dump(index: dict, path: Path) -> None:
    """Write an index into a file."""
    path.write_text(json.dumps(index, indent=2, ensure_ascii=False) + "\n")


def folder(program: str, version: str) -> str:
    """Return the folder of a release's files in the bucket, such as
    oth06/0.1.1."""
    return f"{product_name(program)}/{version}"


def tag(program: str, version: str) -> str:
    """Return the git tag of a release, such as oth06-v0.1.1."""
    return f"{product_name(program)}-v{version}"


def releases(index: dict, program: str) -> list[dict]:
    """Return the releases of a program, newest first."""
    return index["programs"].get(product_name(program), [])


def find(index: dict, program: str, version: str) -> dict | None:
    """Return the release of a program's version, or None."""
    for release in releases(index, program):
        if release["version"] == version:
            return release
    return None


def file_prefix(program: str) -> str:
    """Return the start of the names of a program's files, such as
    oth06- or othpatch-."""
    return f"{product_name(program)}-"


def file_suffix(program: str, platform: str) -> str:
    """Return the end of the name of a program's file for a platform, such
    as -Linux.zip or -Windows.exe."""
    if program != "othpatch":
        return f"-{platform}.zip"
    return f"-{platform}.exe" if platform == "Windows" else f"-{platform}"


def match_files(program: str, version: str | None, names: list[str]) -> dict:
    """Return the names among names that are files of a program, as a map
    from platform to name.

    With a version, only that version's files count. Without one, any
    version counts, as for the development snapshot.
    """
    prefix = file_prefix(program)
    found = {}
    for platform in PLATFORMS:
        suffix = file_suffix(program, platform)
        if version is not None:
            matches = [name for name in names if name == prefix + file_version(version) + suffix]
        else:
            matches = [
                name
                for name in names
                if name.startswith(prefix)
                and name.endswith(suffix)
                and len(name) > len(prefix) + len(suffix)
            ]
        if matches:
            found[platform] = sorted(matches)[-1]
    return found


def file_version_of(program: str, platform: str, name: str) -> str:
    """Return the version in the name of a program's file for a platform,
    as the file name holds it, such as 0.1.2-dev-abcdef0."""
    return name[len(file_prefix(program)) : -len(file_suffix(program, platform))]


def file_entry(path: Path, url: str) -> dict:
    """Return the entry of one file: its name, URL, size, SHA-256 and
    SHA-1."""
    data = path.read_bytes()
    return {
        "name": path.name,
        "url": url,
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "sha1": hashlib.sha1(data).hexdigest(),
    }


def checksum_file(entries: list[dict], algorithm: str) -> str:
    """Return the text of a checksum file, such as SHA256SUMS, for file
    entries: one line "<hash>  <name>" per file, in name order, as
    sha256sum and sha1sum write it."""
    return "".join(
        f"{entry[algorithm]}  {entry['name']}\n"
        for entry in sorted(entries, key=lambda entry: entry["name"])
    )


def utc_time(time: datetime.datetime) -> str:
    """Return a time as the index holds it, such as 2026-11-12T14:03:00Z."""
    time = time.astimezone(datetime.UTC).replace(microsecond=0)
    return time.isoformat().replace("+00:00", "Z")


def replace_error(program: str, release: dict, now: datetime.datetime) -> str | None:
    """Return why a published release cannot be replaced at the time now,
    or None when it can."""
    name = f"{program} {release['version']}"
    if "published" not in release:
        return f"{name} cannot be replaced: the index has no time of its publication"
    published = datetime.datetime.fromisoformat(release["published"])
    if now - published >= REPLACE_WINDOW:
        return (
            f"{name} cannot be replaced: it was published at"
            f" {release['published']}, 24 hours or more ago. Release a new"
            " version instead"
        )
    return None


def version_key(version: str) -> tuple[int, ...]:
    return tuple(int(part) for part in version.split("."))


def add_release(index: dict, program: str, release: dict) -> None:
    """Add the release of a program, keeping the releases newest first.

    Raises ValueError when the index has that version already.
    """
    if find(index, program, release["version"]) is not None:
        raise ValueError(f"the index has {program} {release['version']} already")
    listed = index["programs"].setdefault(product_name(program), [])
    listed.append(release)
    listed.sort(key=lambda item: version_key(item["version"]), reverse=True)


def remove_release(index: dict, program: str, version: str) -> None:
    """Remove the release of a program's version, if the index has it."""
    listed = releases(index, program)
    listed[:] = [item for item in listed if item["version"] != version]
