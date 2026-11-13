# SPDX-License-Identifier: GPL-3.0-only
"""Tests of the changelog format and of tools/lint/check_changelog."""

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
from unittest import mock

TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"
CHECK = TOOLS_DIR / "lint" / "check_changelog"

sys.path.insert(0, str(TOOLS_DIR))

from shared.changelog import check, date, section

LOADER = importlib.machinery.SourceFileLoader("check_changelog", str(CHECK))
CHECK_CHANGELOG = importlib.util.module_from_spec(
    importlib.util.spec_from_loader(LOADER.name, LOADER)
)
LOADER.exec_module(CHECK_CHANGELOG)

GOOD = """# th06 changelog

## Unreleased

- Added a thing.
- Fixed a thing that a release
  shipped.

## 1.1.0 (2026-11-12)

Sums up the release in one paragraph,
over two lines.

- Added one.
- Added two.
- Changed three.
- Removed four.

## 1.0.0 (2026-11-11)

- Added the game.
"""


def git_env() -> dict[str, str]:
    """Return the environment without the variables of the git process and
    the changelog check that the tests run in."""
    return {
        name: value
        for name, value in os.environ.items()
        if not name.startswith("GIT_") and name != "CHANGELOG_AMEND"
    }


def git(repo: Path, *args: str) -> None:
    config = ["-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false"]
    subprocess.run(
        ["git", *config, *args],
        cwd=repo,
        env=git_env(),
        check=True,
        capture_output=True,
    )


class FormatTest(unittest.TestCase):
    def errors(self, text: str) -> list[str]:
        return check(text, "th06")

    def test_good_changelog_passes(self) -> None:
        self.assertEqual(self.errors(GOOD), [])

    def test_changelog_without_unreleased_passes(self) -> None:
        text = GOOD.replace("## Unreleased\n\n- Added a thing.\n", "")
        text = text.replace("- Fixed a thing that a release\n  shipped.\n\n", "")
        self.assertEqual(self.errors(text), [])

    def test_sections_read_the_new_heading(self) -> None:
        self.assertEqual(date(GOOD, "1.1.0"), "2026-11-12")
        self.assertIsNone(date(GOOD, "Unreleased"))
        self.assertTrue(section(GOOD, "1.1.0").startswith("Sums up"))
        self.assertTrue(section(GOOD, "1.1.0").endswith("- Removed four."))
        self.assertEqual(section(GOOD, "1.0.0"), "- Added the game.")
        self.assertIsNone(section(GOOD, "1.0"))

    def test_title_names_the_program(self) -> None:
        errors = self.errors(GOOD.replace("# th06 changelog", "# Changelog"))
        self.assertIn("'# th06 changelog'", errors[0])

    def test_intro_paragraph_fails(self) -> None:
        text = GOOD.replace("# th06 changelog\n", "# th06 changelog\n\nAn intro.\n")
        self.assertIn("without other text", self.errors(text)[0])

    def test_empty_unreleased_fails(self) -> None:
        text = GOOD.replace(
            "- Added a thing.\n- Fixed a thing that a release\n  shipped.\n", ""
        )
        self.assertIn("remove the heading", self.errors(text)[0])

    def test_unreleased_below_a_version_fails(self) -> None:
        text = GOOD + "\n## Unreleased\n\n- Added more.\n"
        self.assertIn("first section", " ".join(self.errors(text)))

    def test_old_heading_fails(self) -> None:
        text = GOOD.replace("## 1.0.0 (2026-11-11)", "## 1.0.0 - 2026-11-11")
        self.assertIn("'## X.Y.Z (YYYY-MM-DD)'", self.errors(text)[0])

    def test_bad_date_fails(self) -> None:
        text = GOOD.replace("(2026-11-11)", "(2026-13-11)")
        self.assertIn("not a date", self.errors(text)[0])

    def test_versions_go_newest_first(self) -> None:
        text = GOOD.replace("## 1.0.0 (", "## 1.1.0 (")
        self.assertIn("newest first", self.errors(text)[0])
        text = GOOD.replace("## 1.0.0 (", "## 1.2.0 (")
        self.assertIn("newest first", self.errors(text)[0])

    def test_subsection_fails(self) -> None:
        text = GOOD.replace("- Added one.", "### Added\n\n- Added one.")
        self.assertIn("without subsections", " ".join(self.errors(text)))

    def test_entry_without_a_verb_fails(self) -> None:
        text = GOOD.replace("- Added one.", "- Adds one.")
        self.assertIn(
            "must start with Added, Changed, Fixed or Removed", self.errors(text)[0]
        )

    def test_entries_out_of_order_fail(self) -> None:
        text = GOOD.replace(
            "- Added two.\n- Changed three.", "- Changed three.\n- Added two."
        )
        self.assertIn("out of order", self.errors(text)[0])

    def test_wrapped_line_needs_two_spaces(self) -> None:
        for indent in ["", " ", "    "]:
            text = GOOD.replace("  shipped.", f"{indent}shipped.")
            self.assertIn("two spaces", self.errors(text)[0], repr(indent))

    def test_blank_line_between_entries_fails(self) -> None:
        text = GOOD.replace("- Added one.\n", "- Added one.\n\n")
        self.assertIn("blank line", self.errors(text)[0])

    def test_two_paragraph_summary_fails(self) -> None:
        text = GOOD.replace("over two lines.\n", "over two lines.\n\nAnd more.\n")
        self.assertIn("one paragraph", self.errors(text)[0])

    def test_section_without_entries_fails(self) -> None:
        text = GOOD.replace("- Added the game.\n", "Only a summary.\n")
        self.assertIn("no list of entries", self.errors(text)[0])


class FrozenTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.dir = tempfile.TemporaryDirectory()
        cls.repo = Path(cls.dir.name)
        cls.path = cls.repo / "docs" / "th06" / "CHANGELOG.md"
        cls.path.parent.mkdir(parents=True)
        git(cls.repo, "init", "-q")
        cls.path.write_text(GOOD)
        git(cls.repo, "add", ".")
        git(cls.repo, "commit", "-q", "-m", "t")

    @classmethod
    def tearDownClass(cls) -> None:
        cls.dir.cleanup()

    def run_check(
        self, text: str, amend: str | None = None, path: Path | None = None
    ) -> subprocess.CompletedProcess:
        path = path or self.path
        path.write_text(text)
        stderr = io.StringIO()
        with (
            mock.patch.dict(
                os.environ, git_env() | {"CHANGELOG_AMEND": amend or ""}, clear=True
            ),
            contextlib.redirect_stderr(stderr),
        ):
            code = CHECK_CHANGELOG.main(["check_changelog", str(path)])
        return subprocess.CompletedProcess([], code, "", stderr.getvalue())

    def test_unchanged_and_new_sections_pass(self) -> None:
        self.assertEqual(self.run_check(GOOD).returncode, 0)
        text = GOOD.replace("## Unreleased", "## 1.2.0 (2026-11-13)")
        result = self.run_check(text)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_unreleased_changes_pass(self) -> None:
        result = self.run_check(
            GOOD.replace("- Added a thing.", "- Added another thing.")
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_changed_released_section_fails(self) -> None:
        result = self.run_check(GOOD.replace("- Added two.", "- Added 2."))
        self.assertEqual(result.returncode, 1)
        self.assertIn("'## 1.1.0 (2026-11-12)' changed", result.stderr)
        self.assertIn("CHANGELOG_AMEND=1.1.0", result.stderr)

    def test_changed_date_fails(self) -> None:
        result = self.run_check(GOOD.replace("(2026-11-11)", "(2026-11-10)"))
        self.assertEqual(result.returncode, 1)
        self.assertIn("'## 1.0.0 (2026-11-11)' changed", result.stderr)

    def test_removed_released_section_fails(self) -> None:
        text = GOOD[: GOOD.index("## 1.0.0")].rstrip() + "\n"
        result = self.run_check(text)
        self.assertEqual(result.returncode, 1)
        self.assertIn("is gone", result.stderr)
        self.assertIn("CHANGELOG_AMEND=1.0.0", result.stderr)

    def test_amend_lets_the_named_sections_change(self) -> None:
        text = GOOD.replace("- Added two.", "- Added 2.").replace("the game.", "it.")
        self.assertEqual(self.run_check(text, "1.1.0").returncode, 1)
        result = self.run_check(text, "1.1.0, 1.0.0")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_file_new_in_the_commit_passes(self) -> None:
        path = self.repo / "docs" / "th07" / "CHANGELOG.md"
        path.parent.mkdir(exist_ok=True)
        text = GOOD.replace("# th06", "# th07").replace("- Added two.", "- Added 2.")
        result = self.run_check(text, path=path)
        self.assertEqual(result.returncode, 0, result.stderr)


class CommandLineTest(unittest.TestCase):
    def test_changed_released_section_fails(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            repo = Path(name)
            path = repo / "docs" / "th06" / "CHANGELOG.md"
            path.parent.mkdir(parents=True)
            git(repo, "init", "-q")
            path.write_text(GOOD)
            git(repo, "add", ".")
            git(repo, "commit", "-q", "-m", "t")
            path.write_text(GOOD.replace("- Added two.", "- Added 2."))
            result = subprocess.run(
                [sys.executable, str(CHECK), str(path)],
                env=git_env(),
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 1)
            self.assertIn("CHANGELOG_AMEND=1.1.0", result.stderr)


if __name__ == "__main__":
    unittest.main()
