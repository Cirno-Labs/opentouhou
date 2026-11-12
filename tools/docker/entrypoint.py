#!/usr/bin/env python3
"""Build and package opentouhou inside a build image.

package writes the game packages into build/packages/, each with the
othpatch that src/<game>/othpatch.txt names, downloaded as the release
index gives it. Without that release, or outside a release build
without the index, the packages carry the othpatch in
build/<platform>/othpatch/<target>/, which the othpatch image built
from the same commit before. package-othpatch writes the executable of
othpatch there, as its release names it.

Each image runs this script with its platform and the programs that it
builds: the media image the games, the othpatch image othpatch. The
games build in build/<platform>/<target>/, othpatch in
build/<platform>/othpatch/<target>/. The repository is mounted at /app.

The environment variable RELEASES_URL, when set, gives the build option
releases_url, and the default in meson.options applies otherwise.
shared/othpatch_release.py names the release index that packaging
reads.
"""

import argparse
import json
import os
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

from shared.othpatch_release import build_releases_url, pinned_othpatch
from shared.versioning import (
    generate_package_name,
    generate_version,
    othpatch_asset_name,
    product_name,
)

REPO_DIR = Path("/app")
PACKAGING_DIR = REPO_DIR / "tools/packaging"
PACKAGES_DIR = REPO_DIR / "build/packages"
CROSS_FILES = {"win": REPO_DIR / "tools/docker/win/meson_cross_x86_64.txt"}
EXE_SUFFIXES = {"linux": "", "win": ".exe"}
GAMES = ["th06"]
CAPTURE_CROSS_FILE = REPO_DIR / "tools/docker/win/meson_cross_i686.txt"
COMMAND_PROGRAMS = {
    "package": "games",
    "package-othpatch": "othpatch",
    "replay": "games",
}


def build_dir(platform: str, programs: str, target: str) -> Path:
    if programs == "othpatch":
        return REPO_DIR / "build" / platform / "othpatch" / target
    return REPO_DIR / "build" / platform / target


def options(programs: str) -> list[str]:
    values = {"programs": programs, "releases_url": build_releases_url(REPO_DIR)}
    return [f"-D{name}={value}" for name, value in values.items()]


def changed_options(root: Path, programs: str) -> list[str]:
    path = root / "meson-info/intro-buildoptions.json"
    configured = {
        f"-D{option['name']}={option['value']}"
        for option in json.loads(path.read_text())
    }
    return [option for option in options(programs) if option not in configured]


def run_build(platform: str, programs: str, target: str) -> None:
    root = build_dir(platform, programs, target)
    if not (root / "build.ninja").exists():
        command = [
            "meson",
            "setup",
            f"--buildtype={target}",
        ]
        if platform in CROSS_FILES:
            command.append(f"--cross-file={CROSS_FILES[platform]}")
        command += options(programs)
        command += [str(root), str(REPO_DIR)]
        subprocess.check_call(command)
    else:
        # Reconfigures a build directory whose options differ: one that
        # builds other programs, such as the whole tree, which needs
        # libraries that this image lacks, or one with another
        # releases_url.
        changed = changed_options(root, programs)
        if changed:
            subprocess.check_call(["meson", "configure", str(root), *changed])
    subprocess.check_call(["meson", "compile", "-C", str(root)])


def run_capture_build(game: str) -> None:
    source_dir = REPO_DIR / "replay" / game
    build_dir = REPO_DIR / "build/replay" / game
    if not (build_dir / "build.ninja").exists():
        # The image sets PKG_CONFIG_PATH for the x86_64 "win" target. It
        # takes priority over the cross file's pkg_config_libdir, which
        # would otherwise make pkg-config resolve the i686 dependencies to
        # the wrong architecture. Clearing it here leaves pkg_config_libdir
        # as the only search path.
        env = dict(os.environ)
        env.pop("PKG_CONFIG_PATH", None)
        subprocess.check_call(
            [
                "meson",
                "setup",
                f"--cross-file={CAPTURE_CROSS_FILE}",
                str(build_dir),
                str(source_dir),
            ],
            env=env,
        )
    subprocess.check_call(["meson", "compile", "-C", str(build_dir)])


def run_test(platform: str, programs: str, target: str) -> None:
    run_build(platform, programs, target)
    subprocess.check_call(
        [
            "meson",
            "test",
            "-C",
            str(build_dir(platform, programs, target)),
            "--print-errorlogs",
        ]
    )


def write_zip(output: Path, files: list[tuple[Path, str]]) -> None:
    """Write files into a zip, keeping the executable bit of each."""
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as handle:
        for source, name in files:
            info = zipfile.ZipInfo.from_file(source, name)
            info.compress_type = zipfile.ZIP_DEFLATED
            handle.writestr(info, source.read_bytes())


def package_linux(
    game: str, exe: Path, othpatch: Path, output: Path, version: str
) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        app_dir = Path(tmp) / f"{game}.AppDir"
        (app_dir / "usr/bin").mkdir(parents=True)
        doc_dir = app_dir / "usr/share/doc" / game
        doc_dir.mkdir(parents=True)

        shutil.copy2(exe, app_dir / "usr/bin" / product_name(game))
        shutil.copy2(PACKAGING_DIR / game / "README.txt", doc_dir / "README.txt")
        desktop = (PACKAGING_DIR / game / "game.desktop").read_text()
        (app_dir / f"{game}.desktop").write_text(
            desktop.rstrip("\n") + f"\nX-AppImage-Version={version}\n"
        )
        shutil.copy2(PACKAGING_DIR / game / "icon.png", app_dir / f"{game}.png")
        (app_dir / "AppRun").symlink_to(f"usr/bin/{product_name(game)}")

        subprocess.check_call(
            [
                "appimagetool",
                "--no-appstream",
                "--runtime-file",
                os.environ["APPIMAGE_RUNTIME"],
                str(app_dir),
                str(Path(tmp) / "app.AppImage"),
            ],
            env={**os.environ, "ARCH": "x86_64"},
        )
        write_zip(
            output,
            [
                (Path(tmp) / "app.AppImage", f"{product_name(game)}.AppImage"),
                (othpatch, "othpatch"),
                (PACKAGING_DIR / game / "README.txt", "README.txt"),
            ],
        )


def package_win(game: str, exe: Path, othpatch: Path, output: Path) -> None:
    files = [
        (exe, f"{product_name(game)}.exe"),
        (othpatch, "othpatch.exe"),
        (PACKAGING_DIR / game / "README.txt", "README.txt"),
    ]
    write_zip(output, files)


def built_othpatch(platform: str, target: str) -> Path:
    path = build_dir(platform, "othpatch", target) / f"othpatch{EXE_SUFFIXES[platform]}"
    if not path.exists():
        raise SystemExit(
            f"{path.relative_to(REPO_DIR)} does not exist. Build othpatch"
            f" first: just build-{platform} {target}"
        )
    return path


def run_package(platform: str, target: str) -> None:
    PACKAGES_DIR.mkdir(parents=True, exist_ok=True)
    for game in GAMES:
        version = generate_version(game, REPO_DIR)
        othpatch = pinned_othpatch(
            game, platform, lambda: built_othpatch(platform, target), REPO_DIR
        )
        name = f"{product_name(game)}{EXE_SUFFIXES[platform]}"
        exe = build_dir(platform, "games", target) / name
        stem = generate_package_name(game, version, platform)
        output = PACKAGES_DIR / f"{stem}.zip"
        if platform == "linux":
            package_linux(game, exe, othpatch, output, version)
        else:
            package_win(game, exe, othpatch, output)
        print(f"Created {output.relative_to(REPO_DIR)}")


def run_package_othpatch(platform: str, target: str) -> None:
    PACKAGES_DIR.mkdir(parents=True, exist_ok=True)
    name = f"othpatch{EXE_SUFFIXES[platform]}"
    exe = build_dir(platform, "othpatch", target) / name
    version = generate_version("othpatch")
    output = PACKAGES_DIR / othpatch_asset_name(version, platform)
    shutil.copy2(exe, output)
    print(f"Created {output.relative_to(REPO_DIR)}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=["linux", "win"], required=True)
    parser.add_argument("--programs", choices=["games", "othpatch"], required=True)
    subparsers = parser.add_subparsers(dest="command", required=True)
    for name in ["build", "test", "package", "package-othpatch"]:
        subparser = subparsers.add_parser(name)
        subparser.add_argument(
            "--target", choices=["debug", "release"], default="debug"
        )
    capture_parser = subparsers.add_parser("replay")
    capture_parser.add_argument("game")
    args = parser.parse_args()
    needed = COMMAND_PROGRAMS.get(args.command, args.programs)
    if needed != args.programs:
        parser.error(f"{args.command} runs in the image of {needed}")

    if args.command == "build":
        run_build(args.platform, args.programs, args.target)
    elif args.command == "test":
        run_test(args.platform, args.programs, args.target)
    elif args.command == "replay":
        run_capture_build(args.game)
    elif args.command == "package-othpatch":
        run_package_othpatch(args.platform, args.target)
    else:
        run_package(args.platform, args.target)


if __name__ == "__main__":
    main()
