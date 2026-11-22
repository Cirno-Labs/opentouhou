"""Check the Discord message that announces releases."""

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from shared.announcement import MESSAGE_LIMIT, MORE, entries, message


class AnnouncementTest(unittest.TestCase):
    def test_wrapped_entries_join_into_one_line(self) -> None:
        changes = "- Added a thing\n  over two lines.\n- Fixed another."
        self.assertEqual(
            entries(changes), ["- Added a thing over two lines.", "- Fixed another."]
        )

    def test_a_summary_paragraph_is_one_entry(self) -> None:
        changes = "A summary\nin two lines.\n\n- Fixed it."
        self.assertEqual(entries(changes), ["A summary in two lines.", "- Fixed it."])

    def test_each_program_gets_its_name_page_and_icon(self) -> None:
        embed = message([("th06", "1.0.2", "- Fixed it.")])["embeds"][0]
        self.assertEqual(embed["title"], "oth06 1.0.2")
        self.assertEqual(embed["url"], "https://opentouhou.org/oth06-releases#v1.0.2")
        self.assertEqual(
            embed["thumbnail"]["url"], "https://opentouhou.org/icons/oth06.webp"
        )
        self.assertEqual(embed["description"], "- Fixed it.")

    def test_othpatch_keeps_its_name(self) -> None:
        embed = message([("othpatch", "1.0.1", "- Fixed it.")])["embeds"][0]
        self.assertEqual(embed["title"], "othpatch 1.0.1")

    def test_the_longest_changelog_loses_its_last_entries(self) -> None:
        short = "- Fixed a small thing."
        long = "\n".join(
            f"- Fixed thing number {i} in some detail." for i in range(400)
        )
        embeds = message([("othpatch", "1.0.1", short), ("th06", "1.0.2", long)])[
            "embeds"
        ]
        self.assertEqual(embeds[0]["description"], short)
        self.assertTrue(embeds[1]["description"].endswith(MORE))
        self.assertIn("- Fixed thing number 0 ", embeds[1]["description"])
        total = sum(len(e["title"]) + len(e["description"]) for e in embeds)
        self.assertLessEqual(total, MESSAGE_LIMIT)


if __name__ == "__main__":
    unittest.main()
