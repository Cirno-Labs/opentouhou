"""Capture presented game windows through the shared session protocol."""

from __future__ import annotations

import argparse
import contextlib
import hashlib
import json
import os
import queue
import re
import signal
import subprocess
import sys
import threading
import time
from datetime import UTC, datetime
from pathlib import Path
from typing import TYPE_CHECKING, TextIO

if TYPE_CHECKING:
    from PIL import Image


def timestamp() -> str:
    return datetime.now(UTC).isoformat()


def save_manifest(out: Path, manifest: dict[str, object]) -> None:
    temporary = out / "manifest.json.tmp"
    temporary.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    temporary.replace(out / "manifest.json")


def stop(process: subprocess.Popen[str], force: bool = False) -> None:
    with contextlib.suppress(ProcessLookupError):
        if os.name == "posix":
            os.killpg(process.pid, signal.SIGKILL if force else signal.SIGTERM)
        elif force:
            process.kill()
        else:
            process.terminate()


def grab_window(rectangle: tuple[int, int, int, int]) -> Image.Image:
    from PIL import ImageGrab

    return ImageGrab.grab(bbox=rectangle).convert("RGB")


def read_output(stream: TextIO, lines: queue.Queue[str | None]) -> None:
    try:
        for line in stream:
            lines.put(line)
    finally:
        lines.put(None)


def capture(
    command: list[str],
    out: Path,
    timeout: float,
    *,
    env: dict[str, str] | None = None,
    quiet: bool = False,
) -> dict[str, object]:
    out.mkdir(parents=True, exist_ok=False)
    command = [str(out / "game") if arg == "{work}" else arg for arg in command]
    manifest: dict[str, object] = {
        "command": command,
        "started_utc": timestamp(),
        "status": "running",
        "captures": [],
    }
    captures: list[dict[str, object]] = []
    fps: list[dict[str, object]] = []
    manifest["fps"] = fps
    manifest["captures"] = captures
    save_manifest(out, manifest)
    process: subprocess.Popen[str] | None = None
    reader: threading.Thread | None = None
    try:
        process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=None,
            text=True,
            bufsize=1,
            start_new_session=os.name == "posix",
            env=env,
        )
        assert process.stdout is not None and process.stdin is not None
        lines: queue.Queue[str | None] = queue.Queue()
        reader = threading.Thread(
            target=read_output, args=(process.stdout, lines), daemon=True
        )
        reader.start()
        ended = False
        last_frame = -1
        with (out / "driver.log").open("w") as log:
            while True:
                try:
                    line = lines.get(timeout=timeout)
                except queue.Empty as error:
                    raise TimeoutError("session stopped reporting progress") from error
                if line is None:
                    break
                log.write(line)
                log.flush()
                parts = line.split()
                if parts[:2] == ["SESSION", "capture"]:
                    if ended or len(parts) != 7:
                        raise ValueError(f"invalid capture request: {line!r}")
                    frame, x, y, width, height = map(int, parts[2:])
                    if frame <= last_frame or width != 640 or height != 480:
                        raise ValueError(f"invalid frame or capture size: {line!r}")
                    # Let the window server finish the acknowledged presentation.
                    time.sleep(0.05)
                    image = grab_window((x, y, x + width, y + height))
                    if image.size != (width, height):
                        raise ValueError("window capture returned the wrong dimensions")
                    name = f"frame-{frame:04d}.png"
                    temporary = out / ("pending-" + name)
                    image.save(temporary)
                    temporary.replace(out / name)
                    captures.append(
                        {
                            "frame": frame,
                            "file": name,
                            "captured_utc": timestamp(),
                            "rgb_sha256": hashlib.sha256(image.tobytes()).hexdigest(),
                            "rectangle": [x, y, width, height],
                        }
                    )
                    process.stdin.write(f"CAPTURED {frame}\n")
                    process.stdin.flush()
                    last_frame = frame
                    if not quiet:
                        print(f"captured {frame}: {out / name}", flush=True)
                elif parts[:2] == ["SESSION", "fps"]:
                    if ended or len(parts) != 4:
                        raise ValueError(f"invalid FPS observation: {line!r}")
                    frame = int(parts[2])
                    token = parts[3]
                    if frame < 0 or (fps and frame <= int(fps[-1]["frame"])):
                        raise ValueError(f"invalid FPS frame: {line!r}")
                    if token == "none" and fps:
                        raise ValueError(
                            f"FPS availability cannot be cleared: {line!r}"
                        )
                    if len(token) > 31 or (
                        token != "none"
                        and re.fullmatch(r"[0-9]+\.[0-9]{2}fps", token) is None
                    ):
                        raise ValueError(f"invalid FPS value: {line!r}")
                    fps.append({"frame": frame, "value": token.removesuffix("fps")})
                elif parts[:2] == ["SESSION", "end"]:
                    if ended or len(parts) != 3 or int(parts[2]) <= last_frame:
                        raise ValueError(f"invalid session end: {line!r}")
                    if fps and int(fps[-1]["frame"]) >= int(parts[2]):
                        raise ValueError("FPS observation is outside the session")
                    manifest["frames"] = int(parts[2])
                    ended = True
        code = process.wait(timeout=timeout)
        if code != 0 or not ended:
            raise RuntimeError(f"session exited with {code}; completed={ended}")
        manifest["status"] = "complete"
    except BaseException as error:
        manifest["status"] = "failed"
        manifest["error"] = str(error)
        raise
    finally:
        if process is not None:
            if process.stdin is not None:
                with contextlib.suppress(OSError):
                    process.stdin.close()
            if os.name == "posix" or process.poll() is None:
                stop(process)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    stop(process, force=True)
                    process.wait()
            if reader is not None:
                reader.join(timeout=1)
            if os.name == "posix":
                stop(process, force=True)
                if reader is not None:
                    reader.join(timeout=1)
            if process.stdout is not None and (reader is None or not reader.is_alive()):
                process.stdout.close()
        manifest["finished_utc"] = timestamp()
        save_manifest(out, manifest)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True, help="new capture directory")
    parser.add_argument("--timeout", type=float, default=120, help="progress timeout")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command
    if command[:1] == ["--"]:
        command = command[1:]
    if not command or args.timeout <= 0:
        parser.error("a command and a positive timeout are required")
    try:
        capture(command, args.out.resolve(), args.timeout)
    except (OSError, ValueError, RuntimeError, TimeoutError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
