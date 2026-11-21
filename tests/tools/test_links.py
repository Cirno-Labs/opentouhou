"""Tests of tools/lint/check_links."""

import importlib.machinery
import importlib.util
import unittest
from pathlib import Path

CHECK = Path(__file__).resolve().parents[2] / "tools" / "lint" / "check_links"

LOADER = importlib.machinery.SourceFileLoader("check_links", str(CHECK))
MODULE = importlib.util.module_from_spec(
    importlib.util.spec_from_loader(LOADER.name, LOADER)
)
LOADER.exec_module(MODULE)


class LinksTest(unittest.TestCase):
    def test_markdown_and_plain_links(self) -> None:
        text = (
            "See [the site](https://opentouhou.org/oth06-manual.html).\n"
            "Downloads: <https://opentouhou.org/>, and\n"
            "https://github.com/Cirno-Labs/opentouhou.\n"
        )
        self.assertEqual(
            MODULE.links(text),
            [
                "https://opentouhou.org/oth06-manual.html",
                "https://opentouhou.org/",
                "https://github.com/Cirno-Labs/opentouhou",
            ],
        )

    def test_each_link_once(self) -> None:
        text = "https://a.example/x and https://a.example/x again"
        self.assertEqual(MODULE.links(text), ["https://a.example/x"])

    def test_placeholders_are_left_out(self) -> None:
        text = "`https://example.org/` and https://dl.example/<version>/file"
        self.assertEqual(MODULE.links(text), ["https://example.org/"])


if __name__ == "__main__":
    unittest.main()
