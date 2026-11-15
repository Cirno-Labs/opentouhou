"""Tests of tools/lint/check_global_reset."""

import importlib.machinery
import importlib.util
import tempfile
import unittest
from pathlib import Path

CHECK = Path(__file__).resolve().parents[2] / "tools" / "lint" / "check_global_reset"

LOADER = importlib.machinery.SourceFileLoader("check_global_reset", str(CHECK))
MODULE = importlib.util.module_from_spec(
    importlib.util.spec_from_loader(LOADER.name, LOADER)
)
LOADER.exec_module(MODULE)


def errors(text: str) -> list[int]:
    return MODULE.errors(text.split("\n"))


class GlobalResetTest(unittest.TestCase):
    def test_global_reset_by_compound_literal_fails(self) -> None:
        self.assertEqual(errors("    g_Bullets = (BULLET_MANAGER) {};\n"), [0])

    def test_module_variable_with_initializer_fails(self) -> None:
        text = "x;\n    m_Input = (TH06_INPUT) {\n        .skip = true,\n    };\n"
        self.assertEqual(errors(text), [1])

    def test_memset_passes(self) -> None:
        self.assertEqual(errors("    memset(&g_Bullets, 0, sizeof(g_Bullets));\n"), [])

    def test_locals_fields_and_elements_pass(self) -> None:
        text = (
            "    pos = (VEC3) { 1.0f, 2.0f, 3.0f };\n"
            "    g_Player.pos = (VEC3) {};\n"
            "    m_Items[i] = (ITEM) {};\n"
        )
        self.assertEqual(errors(text), [])

    def test_check_fails_and_names_the_line(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "a.c"
            path.write_text("void F(void)\n{\n    g_Stage = (STAGE) {};\n}\n")
            self.assertEqual(MODULE.main([str(path)]), 1)
            path.write_text("void F(void)\n{\n}\n")
            self.assertEqual(MODULE.main([str(path)]), 0)


if __name__ == "__main__":
    unittest.main()
