"""Tests of tools/lint/check_module_intro."""

import importlib.machinery
import importlib.util
import tempfile
import unittest
from pathlib import Path

CHECK = Path(__file__).resolve().parents[2] / "tools" / "lint" / "check_module_intro"

LOADER = importlib.machinery.SourceFileLoader("check_module_intro", str(CHECK))
MODULE = importlib.util.module_from_spec(
    importlib.util.spec_from_loader(LOADER.name, LOADER)
)
LOADER.exec_module(MODULE)


def length(text: str) -> int:
    return MODULE.intro_length(text.split("\n"))


class ModuleIntroTest(unittest.TestCase):
    def test_two_line_comment_after_pragma_passes(self) -> None:
        text = "#pragma once\n\n// Holds foo.\n// Used by bar.\n\ntypedef int FOO;\n"
        self.assertEqual(length(text), 2)

    def test_two_line_comment_after_includes_passes(self) -> None:
        text = (
            "#pragma once\n\n#include <stdint.h>\n\n"
            "// Holds foo.\n// Used by bar.\ntypedef int FOO;\n"
        )
        self.assertEqual(length(text), 2)

    def test_one_line_comment_fails(self) -> None:
        text = "#pragma once\n\n// Holds foo.\ntypedef int FOO;\n"
        self.assertEqual(length(text), 1)

    def test_missing_comment_fails(self) -> None:
        text = "#pragma once\n\ntypedef int FOO;\n"
        self.assertEqual(length(text), 0)

    def test_comment_after_a_declaration_does_not_count(self) -> None:
        text = (
            "#pragma once\n\ntypedef int FOO;\n\n"
            "// Holds foo.\n// Used by bar.\nvoid Foo(void);\n"
        )
        self.assertEqual(length(text), 0)

    def test_private_header_is_skipped(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "private.h"
            path.write_text("#pragma once\n\ntypedef int FOO;\n")
            self.assertEqual(MODULE.main([str(path)]), 0)

    def test_private_prefixed_header_is_skipped(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "private_foo.h"
            path.write_text("#pragma once\n\ntypedef int FOO;\n")
            self.assertEqual(MODULE.main([str(path)]), 0)

    def test_public_header_without_an_intro_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "foo.h"
            path.write_text("#pragma once\n\ntypedef int FOO;\n")
            self.assertEqual(MODULE.main([str(path)]), 1)

    def test_public_header_with_an_intro_passes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "foo.h"
            path.write_text(
                "#pragma once\n\n// Holds foo.\n// Used by bar.\n\ntypedef int FOO;\n"
            )
            self.assertEqual(MODULE.main([str(path)]), 0)

    def test_code_right_after_the_intro_fails(self) -> None:
        lines = ["#pragma once", "", "// One.", "// Two.", "int x;"]
        start, length = MODULE.intro_span(lines)
        self.assertEqual((start, length), (2, 2))
        self.assertTrue(lines[start + length].strip())


if __name__ == "__main__":
    unittest.main()
