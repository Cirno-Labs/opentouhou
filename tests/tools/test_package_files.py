"""Check supplemental documents in each native package layout."""

import importlib.machinery
import importlib.util
import json
import os
import sys
import tempfile
import types
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from shared.package_files import package_files


def load_tool(name: str, path: Path) -> types.ModuleType:
    loader = importlib.machinery.SourceFileLoader(name, str(path))
    spec = importlib.util.spec_from_loader(name, loader)
    assert spec is not None
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


DOCKER = load_tool("package_entrypoint", ROOT / "tools/docker/entrypoint.py")
MAC = load_tool("package_mac", ROOT / "tools/mac/build")


class PackageFilesTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.packaging = self.root / "tools/packaging/th99"
        self.packaging.mkdir(parents=True)
        for name in ("oth99_readme.txt", "game.desktop", "icon.png", "icon.icns"):
            (self.packaging / name).write_text(name)
        (self.packaging / "oth99_licenses.txt").write_bytes(b"licenses")
        self.exe = self.root / "exe"
        self.exe.write_bytes(b"executable")
        self.exe.chmod(0o755)
        self.manifest = self.packaging / "files.json"
        self.notice = self.root / "NOTICE"
        self.notice.write_bytes(b"unchanged notice\n")
        self.manifest.write_text(json.dumps([["NOTICE", "licenses/NOTICE.txt"]]))
        self.expected = {"licenses/NOTICE.txt": b"unchanged notice\n"}
        for module in (DOCKER, MAC):
            self.enterContext(patch.object(module, "REPO_DIR", self.root))
            self.enterContext(
                patch.object(module, "PACKAGING_DIR", self.packaging.parent)
            )
        self.enterContext(patch.dict(os.environ, {"APPIMAGE_RUNTIME": "synthetic"}))

    def zip_contents(self, path: Path) -> dict[str, bytes]:
        with zipfile.ZipFile(path) as archive:
            return {name: archive.read(name) for name in archive.namelist()}

    def test_windows_zip_and_no_manifest(self) -> None:
        output = self.root / "windows.zip"
        for extras in (self.expected, {}):
            DOCKER.package_win("th99", self.exe, self.exe, output)
            self.assertEqual(
                self.zip_contents(output),
                {
                    "oth99.exe": b"executable",
                    "othpatch.exe": b"executable",
                    "oth99_readme.txt": b"oth99_readme.txt",
                    "oth99_licenses.txt": b"licenses",
                    **extras,
                },
            )
            self.manifest.unlink(missing_ok=True)

    def test_linux_appdir_and_zip(self) -> None:
        output = self.root / "linux.zip"
        for extras in (self.expected, {}):

            def appimage(
                command: list[str], extras: dict[str, bytes] = extras, **kwargs: object
            ) -> None:
                appdir = Path(command[-2])
                docs = appdir / "usr/share/doc/th99"
                actual = {
                    p.relative_to(docs).as_posix(): p.read_bytes()
                    for p in docs.rglob("*")
                    if p.is_file()
                }
                self.assertEqual(
                    actual,
                    {
                        "oth99_readme.txt": b"oth99_readme.txt",
                        "oth99_licenses.txt": b"licenses",
                        **extras,
                    },
                )
                self.assertEqual((appdir / "AppRun").read_bytes(), b"executable")
                Path(command[-1]).write_bytes(b"synthetic AppImage")

            with patch.object(DOCKER.subprocess, "check_call", side_effect=appimage):
                DOCKER.package_linux("th99", self.exe, self.exe, output, "test")
            self.assertEqual(
                self.zip_contents(output),
                {
                    "oth99.AppImage": b"synthetic AppImage",
                    "othpatch": b"executable",
                    "oth99_readme.txt": b"oth99_readme.txt",
                    "oth99_licenses.txt": b"licenses",
                    **extras,
                },
            )
            self.manifest.unlink(missing_ok=True)

    def test_mac_resources_before_signing(self) -> None:
        for index, extras in enumerate((self.expected, {})):
            bundle = self.root / f"{index}/oth99.app"

            def mac_tool(
                command: list[str],
                bundle: Path = bundle,
                extras: dict[str, bytes] = extras,
            ) -> None:
                if command[0] == "lipo":
                    Path(command[-1]).write_bytes(b"synthetic universal")
                else:
                    self.assertEqual(command[0], "codesign")
                    resources = bundle / "Contents/Resources"
                    actual = {
                        p.relative_to(resources).as_posix(): p.read_bytes()
                        for p in resources.rglob("*")
                        if p.is_file()
                    }
                    self.assertEqual(
                        actual,
                        {
                            "oth99_readme.txt": b"oth99_readme.txt",
                            "oth99_licenses.txt": b"licenses",
                            "th99.icns": b"icon.icns",
                            **extras,
                        },
                    )

            with patch.object(MAC.subprocess, "check_call", side_effect=mac_tool):
                MAC.write_bundle("th99", "test", "debug", bundle)
            self.manifest.unlink(missing_ok=True)

    def test_invalid_manifest_fails_before_packaging(self) -> None:
        invalid = [
            [["../NOTICE", "NOTICE"]],
            [["/NOTICE", "NOTICE"]],
            [["NOTICE", "../NOTICE"]],
            [["NOTICE", "/NOTICE"]],
            [["NOTICE", "C:\\NOTICE"]],
            [["NOTICE", "README.txt"]],
            [["NOTICE", "oth99_readme.txt"]],
            [["NOTICE", "oth99_licenses.txt"]],
            [["NOTICE", "th99.icns"]],
            [["NOTICE", "README.txt\0extra"]],
            [["NOTICE", "NOTICE\nextra"]],
            [["NOTICE", "LICENSE."]],
            [["NOTICE", "LICENSE "]],
            [["NOTICE", "CON.txt"]],
            [["NOTICE", "bad?.txt"]],
            [["NOTICE", "a"], ["NOTICE", "A"]],
            [["NOTICE", "a"], ["NOTICE", "a/b"]],
            [["absent", "NOTICE"]],
            {"NOTICE": "NOTICE"},
            [["NOTICE"]],
        ]
        external = self.root.parent / f"{self.root.name}-external"
        external.write_bytes(b"outside")
        self.addCleanup(external.unlink)
        (self.root / "escape").symlink_to(external)
        invalid.append([["escape", "NOTICE"]])
        for rows in invalid:
            with self.subTest(rows=rows):
                self.manifest.write_text(json.dumps(rows))
                with patch.object(DOCKER.subprocess, "check_call") as command:
                    with self.assertRaises((ValueError, FileNotFoundError)):
                        DOCKER.package_linux(
                            "th99", self.exe, self.exe, self.root / "bad.zip", "test"
                        )
                    with self.assertRaises((ValueError, FileNotFoundError)):
                        MAC.write_bundle("th99", "test", "debug", self.root / "bad.app")
                    with self.assertRaises((ValueError, FileNotFoundError)):
                        DOCKER.package_win(
                            "th99", self.exe, self.exe, self.root / "bad-win.zip"
                        )
                    command.assert_not_called()
                self.assertFalse((self.root / "bad.zip").exists())
                self.assertFalse((self.root / "bad.app").exists())
                self.assertFalse((self.root / "bad-win.zip").exists())

    def test_missing_manifest_leaves_other_games_unchanged(self) -> None:
        self.assertEqual(package_files("th06", self.root), [])


if __name__ == "__main__":
    unittest.main()
