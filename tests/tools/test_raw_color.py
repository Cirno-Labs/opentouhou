"""Tests of tools/lint/check_raw_color."""

import importlib.machinery
import importlib.util
import tempfile
import unittest
from pathlib import Path

CHECK = Path(__file__).resolve().parents[2] / "tools" / "lint" / "check_raw_color"

LOADER = importlib.machinery.SourceFileLoader("check_raw_color", str(CHECK))
MODULE = importlib.util.module_from_spec(
    importlib.util.spec_from_loader(LOADER.name, LOADER)
)
LOADER.exec_module(MODULE)


def errors(text: str) -> list[int]:
    return MODULE.errors(text.split("\n"))


class RawColorTest(unittest.TestCase):
    def test_raw_color_fails(self) -> None:
        text = (
            "    vm->color = 0x80ffffffu;\n"
            "    CHECK(effect->vm.color == 0xff804020u);\n"
            "    AsciiText_SetColor(0xffffffff);\n"
            "#define M_COLOR_SHOCKWAVE 0xff4040ffu\n"
            "    m_FadeColor = 0xffffffff;\n"
        )
        self.assertEqual(errors(text), [0, 1, 2, 3, 4])

    def test_macros_masks_and_other_values_pass(self) -> None:
        text = (
            "    vm->color = COLOR_ARGB(0x80, 0xff, 0xff, 0xff);\n"
            "    vm->color = (vm->color & 0xff000000u) | 0xffffffu;\n"
            "    fade_color = 0xff000000 | rgb;\n"
            "    color &= 0xff000000;\n"
            "    card->header.magic = 0x4b544143;\n"
            "#define M_CHECKSUM_START 0x3f000318u\n"
        )
        self.assertEqual(errors(text), [])

    def test_check_fails_and_names_the_line(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "a.c"
            path.write_text("void F(void)\n{\n    vm->color = 0xff000000;\n}\n")
            self.assertEqual(MODULE.main([str(path)]), 1)
            path.write_text("void F(void)\n{\n}\n")
            self.assertEqual(MODULE.main([str(path)]), 0)


if __name__ == "__main__":
    unittest.main()
