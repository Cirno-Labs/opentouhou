"""Drive capture and replay through one portable case interface."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import cast
from zipfile import BadZipFile, is_zipfile

from shared.replay_case import CaseArchive, import_legacy, legacy_header, write_case
from shared.replay_session import capture

ROOT = Path(__file__).resolve().parents[2]


def native_program(args: argparse.Namespace, game: str) -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    program = args.build_dir.resolve() / (game + "replay" + suffix)
    if not program.is_file():
        raise ValueError(f"build {game}replay or set --build-dir: {program}")
    return program


def language(args: argparse.Namespace) -> str:
    return args.language or ("en" if args.patches or args.thcrap_config else "ja")


def retail_path(path: Path) -> str:
    resolved = path.resolve()
    return str(resolved) if os.name == "nt" else "Z:" + resolved.as_posix()


def command(
    args: argparse.Namespace,
    game: str,
    inputs: Path,
    staged_replay: Path | None,
    work: Path,
    expected: Path | None = None,
) -> tuple[list[str], dict[str, str]]:
    env = dict(os.environ)
    env.setdefault("LP_NUM_THREADS", "1")
    if args.backend == "native":
        result = [
            str(native_program(args, game)),
            "session",
            "--data",
            str(args.data.resolve()),
            "--inputs",
            str(inputs.resolve()),
            "--out",
            str(work),
            "--record-state",
        ]
        if expected is not None:
            result += ["--compare-state", str(expected)]
        patches = args.patches
        if patches is None and language(args) != "ja":
            data_home = Path(
                env.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))
            )
            candidate = data_home / "opentouhou/patches"
            if candidate.is_dir():
                patches = candidate
        if language(args) != "ja" and patches is None:
            raise ValueError("translated native sessions need --patches")
        if patches is not None:
            result += [
                "--patches",
                str(patches.resolve()),
                "--language",
                language(args),
            ]
        if staged_replay is not None:
            result += ["--stage-replay", str(staged_replay.resolve())]
    else:
        program = args.retail_dir.resolve() / game / (game + "capture.exe")
        if not program.is_file():
            raise ValueError(
                f"build the {game} capture shim or set --retail-dir: {program}"
            )
        result = [str(program)]
        if os.name != "nt":
            if shutil.which("wine") is None:
                raise ValueError("retail capture requires Wine")
            result.insert(0, "wine")
            prefix = args.wine_prefix or Path(
                env.get(
                    "WINEPREFIX", str(Path.home() / ".cache/opentouhou/replay-wine")
                )
            )
            env["WINEPREFIX"] = str(prefix.resolve())
            env.setdefault("WINEDEBUG", "-all")
            drivers = "winealsa.drv,winepulse.drv,wineoss.drv,winecoreaudio.drv="
            env["WINEDLLOVERRIDES"] = ";".join(
                value for value in (env.get("WINEDLLOVERRIDES"), drivers) if value
            )
            env["LANG"] = env["LC_ALL"] = "ja_JP.UTF-8"
        result += [
            "--game",
            retail_path(args.data),
            "--inputs",
            retail_path(inputs),
            "--out",
            retail_path(work),
            "--session-state",
        ]
        font = args.font or ROOT / "src/core/text/fonts/msgothic.ttc"
        result += ["--font", retail_path(font)]
        if staged_replay is not None:
            result += ["--stage-replay", retail_path(staged_replay)]
        if (args.thcrap_dll is None) != (args.thcrap_config is None):
            raise ValueError("supply --thcrap-dll and --thcrap-config together")
        if language(args) != "ja" and args.thcrap_config is None:
            raise ValueError(
                "translated retail sessions need a thcrap DLL and configuration"
            )
        if args.thcrap_dll is not None:
            result += [
                "--thcrap-dll",
                retail_path(args.thcrap_dll),
                "--thcrap-config",
                retail_path(args.thcrap_config),
            ]
    if args.headless:
        result.append("--headless")
    if args.fast:
        result.append("--fast")
    if sys.platform.startswith("linux"):
        env.setdefault("SDL_VIDEODRIVER", "x11")
    return result, env


def run_process(
    args: argparse.Namespace,
    game: str,
    inputs: Path,
    staged_replay: Path | None,
    output: Path,
    expected: Path | None = None,
) -> dict[str, object]:
    argv, env = command(args, game, inputs, staged_replay, output / "game", expected)
    if args.backend == "retail" and os.name != "nt":
        Path(env["WINEPREFIX"]).mkdir(parents=True, exist_ok=True)
        subprocess.run(
            ["wineboot", "--init"], env=env, check=True, timeout=args.timeout
        )
    return capture(argv, output, args.timeout, env=env, quiet=True)


def bundle(
    destination: Path,
    game: str,
    inputs: Path,
    staged_replay: Path | None,
    output: Path,
    manifest: dict[str, object],
    args: argparse.Namespace,
) -> None:
    states = sorted((output / "game").glob("*.otrc"))
    if len(states) != 1:
        raise ValueError("session did not finish one complete state trace")
    with states[0].open("rb") as stream:
        header = legacy_header(stream)
    if header.game != game or header.start or "session_frame" not in header.columns:
        raise ValueError("backend did not record the complete session state schema")
    captures = cast(list[dict[str, object]], manifest["captures"])
    screenshots = {int(item["frame"]): output / str(item["file"]) for item in captures}
    write_case(
        destination,
        game=game,
        inputs=inputs,
        input_kind="script",
        states=states,
        screenshots=screenshots,
        staged_replay=staged_replay,
        metadata={
            "backend": args.backend,
            "language": language(args),
            "state_scope": "session",
            "frames": manifest.get("frames"),
            "status": manifest["status"],
            "started_utc": manifest["started_utc"],
            "finished_utc": manifest["finished_utc"],
        },
    )


def record(args: argparse.Namespace) -> int:
    if args.out.exists():
        raise FileExistsError(f"case already exists: {args.out}")
    with tempfile.TemporaryDirectory(prefix="opentouhou-capture-") as folder:
        output = Path(folder) / "session"
        manifest = run_process(args, args.game, args.inputs, args.stage_replay, output)
        bundle(
            args.out, args.game, args.inputs, args.stage_replay, output, manifest, args
        )
    print(f"saved {args.out}")
    return 0


def run(args: argparse.Namespace) -> int:
    if args.out is not None and args.out.exists():
        raise FileExistsError(f"case already exists: {args.out}")
    with (
        CaseArchive(args.case) as archive,
        tempfile.TemporaryDirectory(prefix="opentouhou-replay-") as folder,
    ):
        workspace = Path(folder)
        reference = workspace / "reference"
        archive.extract(reference)
        manifest = archive.manifest
        game = cast(str, manifest["game"])
        inputs = cast(dict[str, str], manifest["inputs"])
        states = cast(list[str], manifest["states"])
        if len(states) != 1:
            raise ValueError("replay requires one complete state trace")
        metadata = manifest.get("metadata", {})
        if args.language is None and isinstance(metadata, dict):
            args.language = metadata.get("language", "ja")
        args.backend = "native"
        if inputs["kind"] == "replay":
            if not args.headless or args.out is not None:
                raise ValueError(
                    "legacy replay cases require --headless "
                    "and have no screenshot timeline"
                )
            result = subprocess.run(
                [
                    str(native_program(args, game)),
                    "--data",
                    str(args.data.resolve()),
                    "--replay",
                    str(reference / inputs["path"]),
                    str(reference / states[0]),
                ],
                timeout=args.timeout,
                check=False,
            )
            return result.returncode
        staged = cast(str | None, manifest.get("staged_replay"))
        staged_path = reference / staged if staged is not None else None
        output = workspace / "actual"
        actual = run_process(
            args,
            game,
            reference / inputs["path"],
            staged_path,
            output,
            reference / states[0],
        )
        if args.out is not None:
            bundle(
                args.out,
                game,
                reference / inputs["path"],
                staged_path,
                output,
                actual,
                args,
            )
            print(f"saved {args.out}")
    print(f"matched {game} session")
    return 0


def inspect(args: argparse.Namespace) -> int:
    if not is_zipfile(args.case):
        with args.case.open("rb") as stream:
            header = legacy_header(stream)
        print(
            json.dumps(
                {
                    "version": 1,
                    "game": header.game,
                    "info": header.info,
                    "start": header.start,
                    "columns": header.columns,
                },
                indent=2,
            )
        )
        return 0
    with CaseArchive(args.case) as archive:
        archive.verify()
        if args.inputs:
            inputs = cast(dict[str, str], archive.manifest["inputs"])
            if inputs["kind"] != "script":
                raise ValueError("legacy input is an embedded binary replay")
            with archive.member(inputs["path"]) as stream:
                print(stream.read().decode("utf-8"), end="")
        else:
            print(json.dumps(archive.manifest, indent=2, ensure_ascii=False))
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    commands = result.add_subparsers(dest="command", required=True)
    capture_parser = commands.add_parser(
        "capture", help="record keys, state and screenshots into one case"
    )
    capture_parser.add_argument("--game", choices=("th06", "th07"), required=True)
    capture_parser.add_argument(
        "--backend", choices=("retail", "native"), default="retail"
    )
    capture_parser.add_argument("--inputs", type=Path, required=True)
    capture_parser.add_argument("--stage-replay", type=Path)
    capture_parser.add_argument("--out", type=Path, required=True)
    replay_parser = commands.add_parser(
        "run", help="replay a portable case and compare its state"
    )
    replay_parser.add_argument("case", type=Path)
    replay_parser.add_argument(
        "--out", type=Path, help="optional case with this run's state and screenshots"
    )
    for command_parser in (capture_parser, replay_parser):
        command_parser.add_argument("--data", type=Path, required=True)
        command_parser.add_argument("--headless", action="store_true")
        command_parser.add_argument("--fast", action="store_true")
        platform = (
            "win" if os.name == "nt" else "mac" if sys.platform == "darwin" else "linux"
        )
        command_parser.add_argument(
            "--build-dir", type=Path, default=ROOT / "build" / platform / "debug"
        )
        command_parser.add_argument(
            "--retail-dir", type=Path, default=ROOT / "build/replay"
        )
        command_parser.add_argument("--wine-prefix", type=Path)
        command_parser.add_argument("--patches", type=Path)
        command_parser.add_argument("--language")
        command_parser.add_argument("--font", type=Path)
        command_parser.add_argument("--thcrap-dll", type=Path)
        command_parser.add_argument("--thcrap-config", type=Path)
        command_parser.add_argument("--timeout", type=float, default=120)
    inspect_parser = commands.add_parser(
        "inspect", help="show readable metadata or input instructions"
    )
    inspect_parser.add_argument("case", type=Path)
    inspect_parser.add_argument("--inputs", action="store_true")
    import_parser = commands.add_parser(
        "import", help="bundle an existing v1 trace and its replay"
    )
    import_parser.add_argument("case", type=Path)
    import_parser.add_argument("--replay", type=Path, required=True)
    import_parser.add_argument("--out", type=Path, required=True)
    return result


def virtual_display(args: argparse.Namespace, argv: list[str] | None) -> int | None:
    needs_window = args.command in ("capture", "run") and (
        args.backend == "retail" if args.command == "capture" else not args.headless
    )
    if args.command == "capture" and not args.headless:
        needs_window = True
    private_display = (
        args.command == "capture" and args.backend == "retail" and args.headless
    )
    if (
        sys.platform.startswith("linux")
        and needs_window
        and (not os.environ.get("DISPLAY") or private_display)
        and not os.environ.get("OPENTOUHOU_REPLAY_XVFB")
    ):
        if shutil.which("xvfb-run") is None:
            raise ValueError("capture without a display requires xvfb-run")
        env = dict(os.environ, OPENTOUHOU_REPLAY_XVFB="1")
        return subprocess.run(
            [
                "xvfb-run",
                "-a",
                "-s",
                "-screen 0 1024x768x24",
                sys.executable,
                str(ROOT / "tools/replay"),
                *(sys.argv[1:] if argv is None else argv),
            ],
            env=env,
            check=False,
        ).returncode
    return None


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    if hasattr(args, "timeout") and args.timeout <= 0:
        print("replay: timeout must be positive", file=sys.stderr)
        return 1
    try:
        display_result = virtual_display(args, argv)
        if display_result is not None:
            return display_result
        if args.command == "capture":
            return record(args)
        if args.command == "run":
            return run(args)
        if args.command == "inspect":
            return inspect(args)
        import_legacy(args.case, args.replay, args.out)
        print(f"saved {args.out}")
        return 0
    except (
        OSError,
        ValueError,
        RuntimeError,
        BadZipFile,
        subprocess.SubprocessError,
    ) as error:
        print(f"replay: {error}", file=sys.stderr)
        return 1
