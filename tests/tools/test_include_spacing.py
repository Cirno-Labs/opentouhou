"""Tests of tools/lint/check_include_spacing."""

import importlib.machinery
import importlib.util
import unittest
from pathlib import Path

CHECK = Path(__file__).resolve().parents[2] / "tools" / "lint" / "check_include_spacing"

LOADER = importlib.machinery.SourceFileLoader("check_include_spacing", str(CHECK))
MODULE = importlib.util.module_from_spec(
    importlib.util.spec_from_loader(LOADER.name, LOADER)
)
LOADER.exec_module(MODULE)


def errors(text: str) -> list[int]:
    return MODULE.errors(text.split("\n"))


class IncludeSpacingTest(unittest.TestCase):
    def test_code_right_after_an_include_fails(self) -> None:
        self.assertEqual(errors('#include "a.h"\nint x;\n'), [0])

    def test_blank_line_after_the_includes_passes(self) -> None:
        self.assertEqual(errors('#include "a.h"\n#include <b.h>\n\nint x;\n'), [])

    def test_preprocessor_line_after_an_include_passes(self) -> None:
        self.assertEqual(errors('#include "a.h"\n#define X 1\n'), [])

    def test_include_right_after_pragma_once_fails(self) -> None:
        self.assertEqual(errors('#pragma once\n#include "a.h"\n\nint x;\n'), [0])

    def test_blank_line_after_pragma_once_passes(self) -> None:
        self.assertEqual(errors('#pragma once\n\n#include "a.h"\n'), [])


if __name__ == "__main__":
    unittest.main()
