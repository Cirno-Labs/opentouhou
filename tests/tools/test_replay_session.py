"""Checks the capture handshake without a display or game data."""

from __future__ import annotations

import contextlib
import importlib.machinery
import importlib.util
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import ModuleType
from unittest.mock import Mock, patch


def load_tool() -> ModuleType:
    path = Path(__file__).resolve().parents[2] / "tools/replay_session"
    loader = importlib.machinery.SourceFileLoader("replay_session", str(path))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    assert spec is not None
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


SESSION = load_tool()


class CaptureTests(unittest.TestCase):
    def test_the_driver_waits_for_the_saved_frame_before_advancing(self) -> None:
        program = (
            "print('SESSION capture 7 3 29 640 480', flush=True)\n"
            "assert input() == 'CAPTURED 7'\n"
            "print('SESSION end 8', flush=True)\n"
        )
        image = Mock()
        image.size = (640, 480)
        image.tobytes.return_value = b"synthetic RGB"
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder) / "capture"

            def save(path: Path) -> None:
                path.write_bytes(b"synthetic image")

            image.save.side_effect = save
            with (
                patch.object(SESSION, "grab_window", return_value=image) as grab,
                contextlib.redirect_stdout(io.StringIO()),
            ):
                result = SESSION.capture([sys.executable, "-c", program], out, 5)
            grab.assert_called_once_with((3, 29, 643, 509))
            self.assertEqual(result["status"], "complete")
            self.assertEqual(result["frames"], 8)
            self.assertEqual(result["captures"][0]["frame"], 7)
            self.assertEqual((out / "frame-0007.png").read_bytes(), b"synthetic image")
            self.assertEqual(json.loads((out / "manifest.json").read_text()), result)

    def test_an_early_exit_is_incomplete_even_with_exit_status_zero(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder) / "capture"
            with self.assertRaisesRegex(RuntimeError, "completed=False"):
                SESSION.capture([sys.executable, "-c", "pass"], out, 5)
            self.assertEqual(
                json.loads((out / "manifest.json").read_text())["status"], "failed"
            )

    def test_invalid_rectangles_and_duplicate_frames_fail(self) -> None:
        programs = [
            "print('SESSION capture 0 0 0 320 240', flush=True)",
            "print('SESSION capture 0 0 0 640 480', flush=True)\n"
            "input()\nprint('SESSION capture 0 0 0 640 480', flush=True)",
        ]
        for program in programs:
            with self.subTest(program=program), tempfile.TemporaryDirectory() as folder:
                image = Mock()
                image.size = (640, 480)
                image.tobytes.return_value = b"RGB"
                with (
                    patch.object(SESSION, "grab_window", return_value=image),
                    contextlib.redirect_stdout(io.StringIO()),
                    self.assertRaises(ValueError),
                ):
                    SESSION.capture(
                        [sys.executable, "-c", program], Path(folder) / "capture", 5
                    )

    def test_a_hung_driver_times_out_and_records_failure(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder) / "capture"
            with self.assertRaises(TimeoutError):
                SESSION.capture(
                    [sys.executable, "-c", "import time; time.sleep(60)"], out, 0.1
                )
            self.assertEqual(
                json.loads((out / "manifest.json").read_text())["status"], "failed"
            )

    def test_an_existing_output_is_never_overwritten(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            sentinel = out / "frame-0000.png"
            sentinel.write_bytes(b"keep")
            with self.assertRaises(FileExistsError):
                SESSION.capture([sys.executable, "-c", "pass"], out, 5)
            self.assertEqual(sentinel.read_bytes(), b"keep")


if __name__ == "__main__":
    unittest.main()
