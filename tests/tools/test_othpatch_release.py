"""Tests of the othpatch that packaging puts into the game packages."""

import contextlib
import hashlib
import io
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from shared.othpatch_release import pinned_othpatch


class PinnedOthpatchTest(unittest.TestCase):
    def setUp(self) -> None:
        self.dir = tempfile.TemporaryDirectory()
        self.root = Path(self.dir.name)
        (self.root / "src/th06").mkdir(parents=True)
        (self.root / "src/th06/othpatch.txt").write_text("0.1.0\n")
        self.built = self.root / "built-othpatch"
        self.built.write_bytes(b"built")
        self.builds = 0

    def tearDown(self) -> None:
        self.dir.cleanup()

    def build(self) -> Path:
        self.builds += 1
        return self.built

    def write_index(self, programs: dict) -> None:
        (self.root / "releases.json").write_text(
            json.dumps({"format": 1, "programs": programs})
        )

    def release(self, data: bytes, digest: str | None = None) -> dict:
        (self.root / "othpatch-0.1.0-Linux").write_bytes(data)
        return {
            "othpatch": [
                {
                    "version": "0.1.0",
                    "files": {
                        "Linux": {
                            "name": "othpatch-0.1.0-Linux",
                            "url": (self.root / "othpatch-0.1.0-Linux").as_uri(),
                            "sha256": digest or hashlib.sha256(data).hexdigest(),
                        }
                    },
                }
            ]
        }

    def pinned(self, release: bool = False) -> tuple[Path, str]:
        env = {
            "RELEASE_INDEX_URL": (self.root / "releases.json").as_uri(),
            "OPENTOUHOU_RELEASE": "1" if release else "",
        }
        output = io.StringIO()
        with mock.patch.dict(os.environ, env), contextlib.redirect_stdout(output):
            path = pinned_othpatch("th06", "linux", self.build, self.root)
        return path, output.getvalue()

    def test_released_version_is_downloaded(self) -> None:
        self.write_index(self.release(b"released"))
        path, output = self.pinned()
        self.assertEqual(path.read_bytes(), b"released")
        self.assertEqual(self.builds, 0)
        self.assertIn("othpatch 0.1.0 from its release", output)

    def test_unreleased_version_takes_the_build_of_the_commit(self) -> None:
        self.write_index({})
        path, output = self.pinned()
        self.assertEqual(path, self.built)
        self.assertEqual(self.builds, 1)
        self.assertIn("othpatch of this commit", output)

    def test_release_build_needs_a_released_othpatch(self) -> None:
        self.write_index({})
        with self.assertRaises(SystemExit) as raised:
            self.pinned(release=True)
        self.assertIn("has no othpatch 0.1.0", str(raised.exception.code))
        self.assertEqual(self.builds, 0)

    def test_wrong_digest_fails(self) -> None:
        self.write_index(self.release(b"released", "0" * 64))
        with self.assertRaises(SystemExit) as raised:
            self.pinned()
        self.assertIn("SHA-256", str(raised.exception.code))


if __name__ == "__main__":
    unittest.main()
