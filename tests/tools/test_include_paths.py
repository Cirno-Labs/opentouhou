"""Tests of tools/lint/check_include_paths."""

import importlib.machinery
import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECK = ROOT / "tools" / "lint" / "check_include_paths"

LOADER = importlib.machinery.SourceFileLoader("check_include_paths", str(CHECK))
MODULE = importlib.util.module_from_spec(
    importlib.util.spec_from_loader(LOADER.name, LOADER)
)
LOADER.exec_module(MODULE)


def error(including: str, name: str) -> str | None:
    return MODULE.error(ROOT / including, name)


class IncludePathsTest(unittest.TestCase):
    def test_paths_from_one_root_pass(self) -> None:
        self.assertIsNone(error("src/th06/main.c", "core/vfs.h"))
        self.assertIsNone(error("tests/th06/test_x.c", "tests/test.h"))
        self.assertIsNone(error("tests/th06/test_x.c", "replay/harness/case.h"))

    def test_generated_header_passes(self) -> None:
        self.assertIsNone(error("src/th06/main.c", "th06/version.h"))

    def test_src_prefix_fails(self) -> None:
        self.assertIsNotNone(error("src/th06/main.c", "src/core/vfs.h"))

    def test_relative_and_bare_paths_fail(self) -> None:
        self.assertIsNotNone(error("src/core/vfs.c", "vfs.h"))
        self.assertIsNotNone(error("src/core/vfs/vfs.c", "../vfs.h"))
        self.assertIsNotNone(error("src/core/vfs.c", "core/./vfs.h"))

    def test_missing_file_fails(self) -> None:
        self.assertIsNotNone(error("src/core/vfs.c", "core/missing.h"))

    def test_root_and_src_do_not_clash(self) -> None:
        self.assertEqual(MODULE.clashes(), [])


if __name__ == "__main__":
    unittest.main()
