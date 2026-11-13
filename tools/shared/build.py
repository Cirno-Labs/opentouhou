# SPDX-License-Identifier: GPL-3.0-only
"""Configure, compile and test the Meson build directories of the programs.

tools/docker/entrypoint.py and tools/mac/build build through these
functions. A build directory builds one set of programs: games, the
games and the replay runner, or othpatch.
"""

import json
import os
import subprocess
from pathlib import Path

from shared.othpatch_release import build_releases_url
from shared.versioning import REPO_DIR

# Names the Meson suites of the tests that every build has, whatever
# programs it builds.
SHARED_SUITES = ["core", "tools"]


def option_values(programs: str, repo_dir: Path = REPO_DIR) -> dict[str, str | bool]:
    """Return the Meson options of a build of programs.

    The environment gives releases_url, as build_releases_url describes,
    and OPENTOUHOU_WERROR=1 turns on werror.
    """
    return {
        "programs": programs,
        "releases_url": build_releases_url(repo_dir),
        "werror": bool(os.environ.get("OPENTOUHOU_WERROR")),
    }


def format_option(name: str, value: str | bool) -> str:
    if isinstance(value, bool):
        value = "true" if value else "false"
    return f"-D{name}={value}"


def changed_options(root: Path, values: dict[str, str | bool]) -> list[str]:
    """Return the options of values that the build directory root has with
    another value."""
    path = root / "meson-info/intro-buildoptions.json"
    configured = {
        option["name"]: option["value"] for option in json.loads(path.read_text())
    }
    return [
        format_option(name, value)
        for name, value in values.items()
        if configured.get(name) != value
    ]


def compile_programs(
    root: Path,
    programs: str,
    target: str,
    cross_file: Path | None = None,
    env: dict[str, str] | None = None,
    repo_dir: Path = REPO_DIR,
) -> None:
    """Configure the build directory root for programs and a target, debug
    or release, then compile it.

    A new directory is set up. An existing one is reconfigured when its
    options differ: one that builds other programs, such as the whole
    tree, which needs libraries that the build may lack, or one with
    another releases_url.
    """
    values = option_values(programs, repo_dir)
    if not (root / "build.ninja").exists():
        command = ["meson", "setup", f"--buildtype={target}"]
        if cross_file is not None:
            command.append(f"--cross-file={cross_file}")
        command += [format_option(name, value) for name, value in values.items()]
        command += [str(root), str(repo_dir)]
        subprocess.check_call(command, env=env)
    else:
        changed = changed_options(root, values)
        if changed:
            subprocess.check_call(["meson", "configure", str(root), *changed], env=env)
    subprocess.check_call(["meson", "compile", "-C", str(root)], env=env)


def run_tests(
    root: Path, skipped: list[str], env: dict[str, str] | None = None
) -> None:
    """Run the tests of the build directory root, except those of the
    Meson suites in skipped."""
    subprocess.check_call(
        [
            "meson",
            "test",
            "-C",
            str(root),
            "--print-errorlogs",
            *(f"--no-suite={suite}" for suite in skipped),
        ],
        env=env,
    )
