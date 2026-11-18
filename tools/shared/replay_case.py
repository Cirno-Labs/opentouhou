"""Store portable replay cases with readable inputs and compressed state traces."""

from __future__ import annotations

import hashlib
import json
import re
import stat
import struct
from collections.abc import Iterator, Mapping, Sequence
from contextlib import contextmanager
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path, PurePosixPath
from typing import BinaryIO
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo

FORMAT = "opentouhou-case"
VERSION = 2
MAX_MANIFEST = 1024 * 1024
MAX_MEMBER = 512 * 1024 * 1024
MAX_TOTAL = 2 * 1024 * 1024 * 1024
MAX_FILES = 4096
BLOCK_SIZE = 65536


@dataclass(frozen=True)
class LegacyHeader:
    game: str
    info: str
    start: dict[str, int]
    columns: tuple[str, ...]


def _read_exact(source: BinaryIO, size: int) -> bytes:
    data = source.read(size)
    if len(data) != size:
        raise ValueError("truncated v1 case header")
    return data


def _u32(source: BinaryIO) -> int:
    return struct.unpack("<I", _read_exact(source, 4))[0]


def _names(source: BinaryIO) -> tuple[str, ...]:
    count = _u32(source)
    if count > 65536:
        raise ValueError("v1 case has too many columns")
    names: list[str] = []
    for _ in range(count):
        size = _read_exact(source, 1)[0]
        name = _read_exact(source, size).decode("utf-8")
        if not name or "\0" in name:
            raise ValueError("invalid v1 case column name")
        names.append(name)
    if len(set(names)) != len(names):
        raise ValueError("repeated v1 case column")
    return tuple(names)


def legacy_header(source: BinaryIO) -> LegacyHeader:
    """Read the v1 schema and checkpoint, leaving the compressed rows unread."""
    if _read_exact(source, 6) != b"OTRC\x01\x00":
        raise ValueError("unsupported v1 case header")
    game_id = _read_exact(source, 8)
    game = game_id.rstrip(b"\0").decode("ascii")
    _game(game)
    size = _u32(source)
    if size > MAX_MANIFEST:
        raise ValueError("v1 case description is too large")
    info = _read_exact(source, size).decode("utf-8")
    names = _names(source)
    values = [_u32(source) for _ in names]
    columns = _names(source)
    if not columns:
        raise ValueError("v1 case has no calc columns")
    return LegacyHeader(game, info, dict(zip(names, values, strict=True)), columns)


def import_legacy(source: Path, replay: Path, destination: Path) -> None:
    """Bundle an existing trace and its replay without recapturing either."""
    with source.open("rb") as stream:
        header = legacy_header(stream)
    write_case(
        destination,
        game=header.game,
        inputs=replay,
        input_kind="replay",
        states=[source],
        metadata={
            "legacy_info": header.info,
            "start": header.start,
            "columns": header.columns,
        },
    )


def _path(name: str) -> None:
    path = PurePosixPath(name)
    if (
        not name
        or not path.parts
        or "\\" in name
        or ":" in name
        or "\0" in name
        or path.is_absolute()
        or path.as_posix() != name
        or any(part in (".", "..") for part in path.parts)
    ):
        raise ValueError(f"invalid case member: {name!r}")


def _game(game: object) -> None:
    if not isinstance(game, str) or not re.fullmatch(r"[a-z][a-z0-9_]{0,7}", game):
        raise ValueError("invalid case game id")


def _constant(value: str) -> object:
    raise ValueError(f"invalid JSON number: {value}")


def write_case(
    destination: Path,
    *,
    game: str,
    inputs: Path,
    input_kind: str,
    states: Sequence[Path],
    screenshots: Mapping[int, Path] | None = None,
    staged_replay: Path | None = None,
    metadata: Mapping[str, object] | None = None,
) -> None:
    """Create a case without changing its sources or replacing an existing file."""
    _game(game)
    if input_kind not in ("script", "replay"):
        raise ValueError("case inputs must be a script or replay")
    input_name = "inputs.txt" if input_kind == "script" else "replay.rpy"
    payloads = {input_name: inputs}
    state_names = [f"states/{index:04d}.otrc" for index in range(len(states))]
    payloads.update(zip(state_names, states, strict=True))
    captures: list[dict[str, object]] = []
    for frame, source in sorted((screenshots or {}).items()):
        if type(frame) is not int or not 0 <= frame <= 0x7FFFFFFF:
            raise ValueError("invalid screenshot frame")
        name = f"screenshots/frame-{frame:08d}.png"
        captures.append({"frame": frame, "path": name})
        payloads[name] = source
    if staged_replay is not None:
        if input_kind != "script":
            raise ValueError("only scripted cases stage a separate replay")
        payloads["replay.rpy"] = staged_replay
    if len(payloads) >= MAX_FILES:
        raise ValueError("case has too many files")
    sizes = {name: source.stat().st_size for name, source in payloads.items()}
    if (
        any(size > MAX_MEMBER for size in sizes.values())
        or sum(sizes.values()) > MAX_TOTAL
    ):
        raise ValueError("case payloads exceed the size limit")
    if input_kind == "script":
        if sizes[input_name] > MAX_MANIFEST:
            raise ValueError("input script exceeds the size limit")
        text = inputs.read_text(encoding="utf-8")
        if "\0" in text:
            raise ValueError("input script contains a NUL byte")
    manifest: dict[str, object] = {
        "format": FORMAT,
        "version": VERSION,
        "game": game,
        "created_utc": datetime.now(UTC).isoformat(),
        "inputs": {"kind": input_kind, "path": input_name},
        "states": state_names,
        "screenshots": captures,
        "staged_replay": "replay.rpy" if staged_replay is not None else None,
        "metadata": dict(metadata or {}),
    }
    # Reject metadata that cannot be represented before creating the output.
    json.dumps(manifest, allow_nan=False)
    created = False
    try:
        with destination.open("xb") as output:
            created = True
            files: dict[str, dict[str, object]] = {}
            total = 0
            with ZipFile(output, "w", compression=ZIP_STORED) as archive:
                for name, source in payloads.items():
                    digest = hashlib.sha256()
                    size = 0
                    info = ZipInfo(name)
                    info.compress_type = (
                        ZIP_DEFLATED if name == "inputs.txt" else ZIP_STORED
                    )
                    with source.open("rb") as reader, archive.open(info, "w") as writer:
                        while block := reader.read(BLOCK_SIZE):
                            size += len(block)
                            total += len(block)
                            if size > MAX_MEMBER or total > MAX_TOTAL:
                                raise ValueError("case payloads exceed the size limit")
                            digest.update(block)
                            writer.write(block)
                    files[name] = {"size": size, "sha256": digest.hexdigest()}
                manifest["files"] = files
                encoded = (
                    json.dumps(manifest, indent=2, allow_nan=False, ensure_ascii=False)
                    + "\n"
                ).encode()
                if len(encoded) > MAX_MANIFEST:
                    raise ValueError("case manifest exceeds the size limit")
                archive.writestr("manifest.json", encoded, compress_type=ZIP_DEFLATED)
    except BaseException:
        if created:
            destination.unlink()
        raise


class CaseArchive:
    """Read a portable case without extracting or loading its large payloads."""

    def __init__(self, path: Path) -> None:
        self.archive = ZipFile(path)
        try:
            self.manifest = self._manifest()
        except BaseException:
            self.archive.close()
            raise

    def _manifest(self) -> dict[str, object]:
        entries = self.archive.infolist()
        if len(entries) > MAX_FILES or len(
            {entry.filename for entry in entries}
        ) != len(entries):
            raise ValueError("case has too many or repeated members")
        total = 0
        for entry in entries:
            _path(entry.filename)
            if entry.is_dir() or stat.S_IFMT(entry.external_attr >> 16) not in (
                0,
                stat.S_IFREG,
            ):
                raise ValueError("case members must be regular files")
            if (
                entry.flag_bits & 1
                or entry.file_size > MAX_MEMBER
                or entry.compress_type not in (ZIP_STORED, ZIP_DEFLATED)
            ):
                raise ValueError("case member is encrypted or too large")
            total += entry.file_size
        if total > MAX_TOTAL:
            raise ValueError("case exceeds the size limit")
        if "manifest.json" not in {entry.filename for entry in entries}:
            raise ValueError("case has no manifest")
        info = self.archive.getinfo("manifest.json")
        if info.file_size > MAX_MANIFEST:
            raise ValueError("case manifest exceeds the size limit")
        manifest = json.loads(self.archive.read(info), parse_constant=_constant)
        if (
            not isinstance(manifest, dict)
            or manifest.get("format") != FORMAT
            or type(manifest.get("version")) is not int
            or manifest.get("version") != VERSION
        ):
            raise ValueError("unsupported case manifest")
        _game(manifest.get("game"))
        files = manifest.get("files")
        if not isinstance(files, dict) or set(files) != {
            entry.filename for entry in entries
        } - {"manifest.json"}:
            raise ValueError("case manifest does not list every payload")
        for name, value in files.items():
            if (
                not isinstance(value, dict)
                or type(value.get("size")) is not int
                or value["size"] != self.archive.getinfo(name).file_size
                or not isinstance(value.get("sha256"), str)
                or not re.fullmatch(r"[0-9a-f]{64}", value["sha256"])
            ):
                raise ValueError(f"invalid payload record: {name}")
        inputs = manifest.get("inputs")
        if (
            not isinstance(inputs, dict)
            or inputs.get("kind") not in ("script", "replay")
            or not isinstance(inputs.get("path"), str)
            or inputs["path"] not in files
        ):
            raise ValueError("case has no input timeline or replay")
        if inputs["kind"] == "script" and files[inputs["path"]]["size"] > MAX_MANIFEST:
            raise ValueError("input script exceeds the size limit")
        states = manifest.get("states")
        if not isinstance(states, list) or any(
            not isinstance(name, str) or name not in files for name in states
        ):
            raise ValueError("invalid case state traces")
        screenshots = manifest.get("screenshots")
        if not isinstance(screenshots, list):
            raise ValueError("invalid case screenshots")
        previous = -1
        for capture in screenshots:
            if (
                not isinstance(capture, dict)
                or type(capture.get("frame")) is not int
                or not previous < capture["frame"] <= 0x7FFFFFFF
                or not isinstance(capture.get("path"), str)
                or capture["path"] not in files
            ):
                raise ValueError("invalid case screenshot record")
            previous = capture["frame"]
        staged = manifest.get("staged_replay")
        if staged is not None and (
            not isinstance(staged, str)
            or staged not in files
            or inputs["kind"] != "script"
        ):
            raise ValueError("invalid staged replay")
        return manifest

    def close(self) -> None:
        self.archive.close()

    def __enter__(self) -> CaseArchive:
        return self

    def __exit__(self, *_args: object) -> None:
        self.close()

    @contextmanager
    def member(self, name: str) -> Iterator[BinaryIO]:
        files = self.manifest["files"]
        assert isinstance(files, dict)
        if name not in files:
            raise ValueError(f"case has no payload {name!r}")
        with self.archive.open(name) as stream:
            yield stream

    def copy_member(self, name: str, destination: BinaryIO | None = None) -> None:
        files = self.manifest["files"]
        assert isinstance(files, dict)
        digest = hashlib.sha256()
        size = 0
        with self.member(name) as source:
            while block := source.read(BLOCK_SIZE):
                digest.update(block)
                size += len(block)
                if destination is not None:
                    destination.write(block)
        if size != files[name]["size"] or digest.hexdigest() != files[name]["sha256"]:
            raise ValueError(f"case payload checksum differs: {name}")

    def verify(self) -> None:
        files = self.manifest["files"]
        assert isinstance(files, dict)
        for name in files:
            self.copy_member(name)

    def extract(self, destination: Path) -> None:
        """Extract verified members into a new directory, including the manifest."""
        destination.mkdir(parents=True, exist_ok=False)
        files = self.manifest["files"]
        assert isinstance(files, dict)
        for name in files:
            target = destination.joinpath(*PurePosixPath(name).parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("xb") as output:
                self.copy_member(name, output)
        (destination / "manifest.json").write_text(
            json.dumps(self.manifest, indent=2) + "\n", encoding="utf-8"
        )
