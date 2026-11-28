"""Tests of tools/lint/check_test_includes."""

import importlib.machinery
import importlib.util
import tempfile
import unittest
from pathlib import Path

CHECK = Path(__file__).resolve().parents[2] / "tools" / "lint" / "check_test_includes"

LOADER = importlib.machinery.SourceFileLoader("check_test_includes", str(CHECK))
MODULE = importlib.util.module_from_spec(
    importlib.util.spec_from_loader(LOADER.name, LOADER)
)
LOADER.exec_module(MODULE)


class TestIncludesTest(unittest.TestCase):
    def test_including_a_c_file_fails(self) -> None:
        text = '#include "th02/gameplay/gameplay.c"\n'
        self.assertEqual(MODULE.errors(text.split("\n")), [0])

    def test_including_a_header_passes(self) -> None:
        text = '#include "th02/gameplay/private.h"\n'
        self.assertEqual(MODULE.errors(text.split("\n")), [])

    def test_check_fails_and_names_the_file(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test_th02_foo.c"
            path.write_text('#include "th02/foo.c"\n')
            self.assertEqual(MODULE.main([str(path)]), 1)
            path.write_text('#include "th02/foo.h"\n')
            self.assertEqual(MODULE.main([str(path)]), 0)


if __name__ == "__main__":
    unittest.main()
