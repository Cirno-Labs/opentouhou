"""Tests of the changelog format and of tools/lint/check_changelog."""

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"
CHECK = TOOLS_DIR / "lint" / "check_changelog"

sys.path.insert(0, str(TOOLS_DIR))

from shared.changelog import check, date, section

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
        text = GOOD.replace("- Added a thing.\n- Fixed a thing that a release\n  shipped.\n", "")
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
        self.assertIn("must start with Added, Changed, Fixed or Removed", self.errors(text)[0])

    def test_entries_out_of_order_fail(self) -> None:
        text = GOOD.replace("- Added two.\n- Changed three.", "- Changed three.\n- Added two.")
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
    def setUp(self) -> None:
        self.dir = tempfile.TemporaryDirectory()
        self.repo = Path(self.dir.name)
        self.path = self.repo / "docs" / "th06" / "CHANGELOG.md"
        self.path.parent.mkdir(parents=True)
        self.git("init", "-q")
        self.path.write_text(GOOD)
        self.git("add", ".")
        self.git("commit", "-q", "-m", "t")

    def tearDown(self) -> None:
        self.dir.cleanup()

    def git(self, *args: str) -> None:
        config = ["-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false"]
        subprocess.run(
            ["git", *config, *args], cwd=self.repo, env=self.env(), check=True, capture_output=True
        )

    def env(self) -> dict:
        return {
            name: value
            for name, value in os.environ.items()
            if not name.startswith("GIT_") and name != "CHANGELOG_AMEND"
        }

    def run_check(self, text: str, amend: str | None = None) -> subprocess.CompletedProcess:
        self.path.write_text(text)
        env = self.env()
        if amend is not None:
            env["CHANGELOG_AMEND"] = amend
        return subprocess.run(
            [sys.executable, str(CHECK), str(self.path)],
            env=env,
            capture_output=True,
            text=True,
        )

    def test_unchanged_and_new_sections_pass(self) -> None:
        self.assertEqual(self.run_check(GOOD).returncode, 0)
        text = GOOD.replace("## Unreleased", "## 1.2.0 (2026-11-13)")
        result = self.run_check(text)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_unreleased_changes_pass(self) -> None:
        result = self.run_check(GOOD.replace("- Added a thing.", "- Added another thing."))
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
        self.git("rm", "-q", "--cached", str(self.path))
        self.git("commit", "-q", "-m", "t")
        result = self.run_check(GOOD.replace("- Added two.", "- Added 2."))
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
