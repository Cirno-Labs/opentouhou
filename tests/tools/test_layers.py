"""Tests of tools/lint/check_layers."""

import importlib.machinery
import importlib.util
import unittest
from pathlib import Path

CHECK = Path(__file__).resolve().parents[2] / "tools" / "lint" / "check_layers"

LOADER = importlib.machinery.SourceFileLoader("check_layers", str(CHECK))
MODULE = importlib.util.module_from_spec(
    importlib.util.spec_from_loader(LOADER.name, LOADER)
)
LOADER.exec_module(MODULE)


def errors(path: str, text: str) -> list[int]:
    return [index for index, _ in MODULE.errors(path, text.split("\n"))]


class LayersTest(unittest.TestCase):
    def test_own_layer_and_dependencies_pass(self) -> None:
        text = (
            '#include "core/vfs.h"\n#include "shell/game.h"\n#include "th06/th06.h"\n'
        )
        self.assertEqual(errors("src/th06/stage/stage.c", text), [])

    def test_layer_above_fails(self) -> None:
        self.assertEqual(errors("src/core/vfs.c", '#include "shell/game.h"\n'), [0])

    def test_other_game_fails(self) -> None:
        self.assertEqual(errors("src/th06/main.c", '#include "th07/th07.h"\n'), [0])

    def test_othpatch_without_shell_game(self) -> None:
        text = '#include "shell/folders.h"\n#include "shell/game.h"\n'
        self.assertEqual(errors("src/othpatch/main.c", text), [1])

    def test_test_helpers_but_not_other_areas(self) -> None:
        text = '#include "tests/test.h"\n#include "tests/th07/th07_fixture.h"\n'
        self.assertEqual(errors("tests/th06/test_x.c", text), [1])

    def test_directory_without_entry_fails(self) -> None:
        self.assertEqual(errors("tests/new/test_x.c", ""), [0])

    def test_clang_format_groups_the_shared_layers(self) -> None:
        style = (CHECK.parents[2] / ".clang-format").read_text(encoding="utf-8")
        group = '^"(' + "|".join(MODULE.SHARED) + ")/"
        self.assertIn(f"- Regex: '{group}'", style)


if __name__ == "__main__":
    unittest.main()
