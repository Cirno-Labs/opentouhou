"""Tests of the program versions: tools/shared/versioning.py, tools/bump
and tools/lint/check_versions, above all for development versions such
as 0.1.0-dev."""

import contextlib
import importlib.machinery
import importlib.util
import io
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import ModuleType
from unittest import mock

TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"

sys.path.insert(0, str(TOOLS_DIR))

from shared import versioning
from shared.changelog import section


def load_tool(path: Path) -> ModuleType:
    """Load a tool, a script without .py, as a module."""
    loader = importlib.machinery.SourceFileLoader(path.name, str(path))
    module = importlib.util.module_from_spec(
        importlib.util.spec_from_loader(loader.name, loader)
    )
    loader.exec_module(module)
    return module


BUMP = load_tool(TOOLS_DIR / "bump")
CHECK_VERSIONS = load_tool(TOOLS_DIR / "lint" / "check_versions")

RELEASED = """# {program} changelog

## Unreleased

- Fixed a thing.

## 1.0.0 (2026-11-13)

- Added the program.
"""
IN_DEVELOPMENT = "# {program} changelog\n"


def write_program(
    repo: Path, program: str, version: str, changelog: str, pin: str | None = None
) -> None:
    (repo / "src" / program).mkdir(parents=True, exist_ok=True)
    (repo / "src" / program / "version.txt").write_text(f"{version}\n")
    if pin is not None:
        (repo / "src" / program / "othpatch.txt").write_text(f"{pin}\n")
    (repo / "docs" / program).mkdir(parents=True, exist_ok=True)
    (repo / "docs" / program / "CHANGELOG.md").write_text(
        changelog.format(program=program)
    )


def run(main, *args) -> tuple[int, str, str]:
    """Run the main function of a tool and return its exit code, output
    and error output."""
    stdout, stderr = io.StringIO(), io.StringIO()
    code = 0
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        try:
            result = main(*args)
            code = result if isinstance(result, int) else 0
        except SystemExit as error:
            code = error.code
    if isinstance(code, str):
        stderr.write(code + "\n")
        code = 1
    return code or 0, stdout.getvalue(), stderr.getvalue()


class RepoTest(unittest.TestCase):
    """Gives each test a repository with othpatch 1.0.0, th06 1.0.0 and
    th07 0.1.0-dev."""

    def setUp(self) -> None:
        self.dir = tempfile.TemporaryDirectory()
        self.repo = Path(self.dir.name)
        write_program(self.repo, "othpatch", "1.0.0", RELEASED)
        write_program(self.repo, "th06", "1.0.0", RELEASED, pin="1.0.0")
        write_program(self.repo, "th07", "0.1.0-dev", IN_DEVELOPMENT, pin="1.0.0")
        patcher = mock.patch.dict(os.environ, {versioning.RELEASE_ENV: ""})
        patcher.start()
        self.addCleanup(patcher.stop)

    def tearDown(self) -> None:
        self.dir.cleanup()

    def read(self, path: str) -> str:
        return (self.repo / path).read_text()


class VersionTest(RepoTest):
    def test_dev_version_has_the_dev_suffix(self) -> None:
        self.assertTrue(versioning.is_dev("0.1.0-dev"))
        self.assertFalse(versioning.is_dev("1.0.0"))

    def test_program_version_accepts_dev_and_nothing_else(self) -> None:
        self.assertEqual(versioning.program_version("th07", self.repo), "0.1.0-dev")
        for text in ["0.1.0-rc1", "0.1.0-dev+abcdef0", "0.1.0-", "0.1"]:
            (self.repo / "src/th07/version.txt").write_text(f"{text}\n")
            with self.assertRaises(ValueError):
                versioning.program_version("th07", self.repo)

    def test_othpatch_pin_is_never_a_dev_version(self) -> None:
        (self.repo / "src/th07/othpatch.txt").write_text("1.0.0-dev\n")
        with self.assertRaises(ValueError):
            versioning.read_version(versioning.othpatch_pin_path("th07", self.repo))

    def git_commit(self, *args, **kwargs) -> subprocess.CompletedProcess:
        return subprocess.CompletedProcess(args, 0, "abcdef0\n", "")

    def test_build_adds_the_commit(self) -> None:
        with mock.patch.object(versioning.subprocess, "run", self.git_commit):
            self.assertEqual(
                versioning.generate_version("th06", self.repo), "1.0.0-dev+abcdef0"
            )
            self.assertEqual(
                versioning.generate_version("th07", self.repo), "0.1.0-dev+abcdef0"
            )

    def test_build_without_git_adds_no_commit(self) -> None:
        with mock.patch.object(versioning.subprocess, "run", side_effect=OSError):
            self.assertEqual(
                versioning.generate_version("th06", self.repo), "1.0.0-dev"
            )
            self.assertEqual(
                versioning.generate_version("th07", self.repo), "0.1.0-dev"
            )

    def test_release_build_of_a_dev_version_fails(self) -> None:
        with mock.patch.dict(os.environ, {versioning.RELEASE_ENV: "1"}):
            self.assertEqual(versioning.generate_version("th06", self.repo), "1.0.0")
            with self.assertRaisesRegex(ValueError, "th07 0.1.0-dev is a development"):
                versioning.generate_version("th07", self.repo)

    def test_release_build_compiles_a_dev_version_without_packaging_it(self) -> None:
        with mock.patch.dict(os.environ, {versioning.RELEASE_ENV: "1"}):
            self.assertEqual(versioning.build_version("th06", self.repo), "1.0.0")
            self.assertEqual(versioning.build_version("th07", self.repo), "0.1.0-dev")
            self.assertEqual(versioning.packaged_games(self.repo), ["th06"])

    def test_other_builds_package_dev_versions(self) -> None:
        with mock.patch.object(versioning.subprocess, "run", self.git_commit):
            self.assertEqual(
                versioning.build_version("th07", self.repo), "0.1.0-dev+abcdef0"
            )
        self.assertEqual(versioning.packaged_games(self.repo), ["th06", "th07"])


class BumpTest(RepoTest):
    def bump(self, *args: str) -> tuple[int, str, str]:
        return run(BUMP.main, list(args), self.repo)

    def test_bump_to_a_dev_version_fails(self) -> None:
        code, _, stderr = self.bump("th06", "1.1.0-dev")
        self.assertEqual(code, 1)
        self.assertIn("without -dev", stderr)
        self.assertEqual(self.read("src/th06/version.txt"), "1.0.0\n")

    def test_with_games_pins_a_dev_game_without_releasing_it(self) -> None:
        code, stdout, stderr = self.bump("othpatch", "1.0.1", "--with-games")
        self.assertEqual(code, 0, stderr)
        self.assertEqual(self.read("src/th06/othpatch.txt"), "1.0.1\n")
        self.assertEqual(self.read("src/th06/version.txt"), "1.0.1\n")
        self.assertIn(
            "- Changed the packages to ship othpatch 1.0.1.",
            section(self.read("docs/th06/CHANGELOG.md"), "1.0.1"),
        )
        self.assertEqual(self.read("src/th07/othpatch.txt"), "1.0.1\n")
        self.assertEqual(self.read("src/th07/version.txt"), "0.1.0-dev\n")
        self.assertEqual(self.read("docs/th07/CHANGELOG.md"), "# th07 changelog\n")
        self.assertIn("Changed src/th07/othpatch.txt", stdout)
        self.assertNotIn("src/th07/version.txt", stdout)

    def test_first_release_of_a_dev_program(self) -> None:
        (self.repo / "docs/th07/CHANGELOG.md").write_text(
            "# th07 changelog\n\n## Unreleased\n\n- Added th07.\n"
        )
        code, _, stderr = self.bump("th07", "0.1.0")
        self.assertEqual(code, 0, stderr)
        self.assertEqual(self.read("src/th07/version.txt"), "0.1.0\n")
        self.assertEqual(
            section(self.read("docs/th07/CHANGELOG.md"), "0.1.0"), "- Added th07."
        )

    def test_release_without_unreleased_changes_keeps_the_version(self) -> None:
        code, _, stderr = self.bump("th07", "0.1.0")
        self.assertEqual(code, 1)
        self.assertIn("has no unreleased changes", stderr)
        self.assertEqual(self.read("src/th07/version.txt"), "0.1.0-dev\n")


class CheckVersionsTest(RepoTest):
    def check(self) -> tuple[int, str, str]:
        return run(CHECK_VERSIONS.main, self.repo)

    def test_dev_version_needs_no_changelog_section(self) -> None:
        code, _, stderr = self.check()
        self.assertEqual(code, 0, stderr)

    def test_dev_version_needs_a_changelog(self) -> None:
        (self.repo / "docs/th07/CHANGELOG.md").unlink()
        code, _, stderr = self.check()
        self.assertEqual(code, 1)
        self.assertIn("docs/th07/CHANGELOG.md does not exist", stderr)

    def test_other_pre_release_parts_fail(self) -> None:
        (self.repo / "src/th07/version.txt").write_text("0.1.0-beta\n")
        code, _, stderr = self.check()
        self.assertEqual(code, 1)
        self.assertIn("X.Y.Z-dev", stderr)

    def test_release_version_needs_a_changelog_section(self) -> None:
        (self.repo / "src/th07/version.txt").write_text("0.1.0\n")
        code, _, stderr = self.check()
        self.assertEqual(code, 1)
        self.assertIn("no section ## 0.1.0", stderr)


if __name__ == "__main__":
    unittest.main()
