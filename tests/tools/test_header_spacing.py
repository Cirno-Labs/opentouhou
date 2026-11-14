"""Tests of tools/lint/check_header_spacing."""

import importlib.machinery
import importlib.util
import tempfile
import unittest
from pathlib import Path

FOO = "// foo\nvoid Foo(void);\n"
BAR = "// bar\nvoid Bar(void);\n"

CHECK = Path(__file__).resolve().parents[2] / "tools" / "lint" / "check_header_spacing"

LOADER = importlib.machinery.SourceFileLoader("check_header_spacing", str(CHECK))
MODULE = importlib.util.module_from_spec(
    importlib.util.spec_from_loader(LOADER.name, LOADER)
)
LOADER.exec_module(MODULE)


def errors(text: str) -> list[int]:
    return MODULE.errors(text.split("\n"))


class HeaderSpacingTest(unittest.TestCase):
    def test_commented_function_after_code_fails(self) -> None:
        self.assertEqual(errors(FOO + BAR), [2])

    def test_blank_line_above_the_comment_passes(self) -> None:
        self.assertEqual(errors(FOO + "\n" + BAR), [])

    def test_comment_at_the_start_passes(self) -> None:
        self.assertEqual(errors("// foo\nvoid Foo(void);\n"), [])

    def test_declaration_over_several_lines_counts(self) -> None:
        text = "void Foo(\n    int a);\n// bar\nRESULT Bar(\n    int b);\n"
        self.assertEqual(errors(text), [2])

    def test_comment_above_two_functions_keeps_them_together(self) -> None:
        text = "// both\nvoid Foo(void);\nvoid Bar(void);\n"
        self.assertEqual(errors(text), [])

    def test_other_declarations_pass(self) -> None:
        text = (
            "int a;\n// type\ntypedef void (*FN)(void);\n"
            "#define X 1\n// global\nextern const int g_Value;\n"
            "struct S {\n    int a;\n    // field\n    void (*f)(void);\n};\n"
        )
        self.assertEqual(errors(text), [])

    def test_fix_inserts_the_blank_lines(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "a.h"
            path.write_text("void Foo(void);\n// bar\nvoid Bar(void);\n")
            self.assertEqual(MODULE.main(["--fix", str(path)]), 0)
            self.assertEqual(
                path.read_text(), "void Foo(void);\n\n// bar\nvoid Bar(void);\n"
            )
            self.assertEqual(MODULE.main([str(path)]), 0)

    def test_check_fails_and_names_the_line(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "a.h"
            path.write_text("void Foo(void);\n// bar\nvoid Bar(void);\n")
            self.assertEqual(MODULE.main([str(path)]), 1)


if __name__ == "__main__":
    unittest.main()
