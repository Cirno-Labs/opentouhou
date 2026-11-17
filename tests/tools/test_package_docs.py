"""Check that packaged documents cannot replace the original game readme."""

import importlib.machinery
import importlib.util
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

from shared.package_docs import package_documents


def load_tool(name: str, path: Path) -> types.ModuleType:
    loader = importlib.machinery.SourceFileLoader(name, str(path))
    spec = importlib.util.spec_from_loader(name, loader)
    assert spec is not None
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


DOCKER = load_tool("document_docker", ROOT / "tools/docker/entrypoint.py")
MAC = load_tool("document_mac", ROOT / "tools/mac/build")


class PackageDocumentsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.packaging = self.root / "tools/packaging"
        self.packaging.mkdir(parents=True)
        for game in ("th01", "th06"):
            folder = self.packaging / game
            folder.mkdir()
            (folder / f"o{game}_readme.txt").write_bytes(game.encode())
            (folder / f"o{game}_licenses.txt").write_bytes(
                self.expected(game)[f"o{game}_licenses.txt"]
            )
            for name in ("game.desktop", "icon.png", "icon.icns"):
                (folder / name).write_text(name)
        self.exe = self.root / "executable"
        self.exe.write_bytes(b"executable")
        self.exe.chmod(0o755)
        for module in (DOCKER, MAC):
            self.enterContext(patch.object(module, "REPO_DIR", self.root))
            self.enterContext(patch.object(module, "PACKAGING_DIR", self.packaging))
        self.enterContext(patch.dict(os.environ, {"APPIMAGE_RUNTIME": "synthetic"}))

    def expected(self, game: str) -> dict[str, bytes]:
        return {
            f"o{game}_readme.txt": game.encode(),
            f"o{game}_licenses.txt": b"common notices\n"
            + (b"\n\nTH01 notices\n" if game == "th01" else b""),
        }

    def test_windows_extract_preserves_original_and_other_game(self) -> None:
        destination = self.root / "game"
        destination.mkdir()
        original = destination / "README.txt"
        original.write_bytes(b"original game readme")
        for game in ("th01", "th06"):
            with self.subTest(game=game):
                output = self.root / f"{game}.zip"
                DOCKER.package_win(game, self.exe, self.exe, output)
                with zipfile.ZipFile(output) as archive:
                    self.assertEqual(
                        set(archive.namelist()),
                        {f"o{game}.exe", "othpatch.exe", *self.expected(game)},
                    )
                    archive.extractall(destination)
                self.assertEqual(original.read_bytes(), b"original game readme")
                for name, content in self.expected(game).items():
                    self.assertEqual((destination / name).read_bytes(), content)
        for game in ("th01", "th06"):
            self.assertEqual(package_documents(game, self.root), self.expected(game))
            for name, content in self.expected(game).items():
                self.assertEqual((destination / name).read_bytes(), content)

    def test_linux_zip_and_appimage_have_same_documents(self) -> None:
        for game in ("th01", "th06"):
            with self.subTest(game=game):

                def appimage(
                    command: list[str], game: str = game, **kwargs: object
                ) -> None:
                    folder = Path(command[-2]) / "usr/share/doc" / game
                    self.assertEqual(
                        {p.name: p.read_bytes() for p in folder.iterdir()},
                        self.expected(game),
                    )
                    Path(command[-1]).write_bytes(b"synthetic AppImage")

                output = self.root / f"{game}-linux.zip"
                with patch.object(
                    DOCKER.subprocess, "check_call", side_effect=appimage
                ):
                    DOCKER.package_linux(game, self.exe, self.exe, output, "test")
                with zipfile.ZipFile(output) as archive:
                    self.assertEqual(
                        set(archive.namelist()),
                        {f"o{game}.AppImage", "othpatch", *self.expected(game)},
                    )
                    for name, content in self.expected(game).items():
                        self.assertEqual(archive.read(name), content)

    def test_mac_documents_are_present_before_signing(self) -> None:
        for game in ("th01", "th06"):
            with self.subTest(game=game):
                bundle = self.root / f"o{game}.app"

                def mac_tool(
                    command: list[str], bundle: Path = bundle, game: str = game
                ) -> None:
                    if command[0] == "lipo":
                        Path(command[-1]).write_bytes(b"synthetic universal")
                    else:
                        self.assertEqual(command[0], "codesign")
                        resources = bundle / "Contents/Resources"
                        self.assertEqual(
                            {p.name: p.read_bytes() for p in resources.iterdir()},
                            {f"{game}.icns": b"icon.icns", **self.expected(game)},
                        )

                with patch.object(MAC.subprocess, "check_call", side_effect=mac_tool):
                    MAC.write_bundle(game, "test", "debug", bundle)


if __name__ == "__main__":
    unittest.main()
