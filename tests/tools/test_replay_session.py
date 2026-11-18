"""Checks the capture handshake without a display or game data."""

from __future__ import annotations

import contextlib
import io
import json
import os
import signal
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from shared import replay_session as SESSION


def fake_image() -> Mock:
    image = Mock()
    image.size = (640, 480)
    image.tobytes.return_value = b"synthetic RGB"

    def save(path: Path) -> None:
        path.write_bytes(b"synthetic image")

    image.save.side_effect = save
    return image


class CaptureTests(unittest.TestCase):
    def test_the_driver_waits_for_the_saved_frame_before_advancing(self) -> None:
        program = (
            "print('SESSION capture 7 3 29 640 480', flush=True)\n"
            "assert input() == 'CAPTURED 7'\n"
            "print('SESSION end 8', flush=True)\n"
        )
        image = fake_image()
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder) / "capture"

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

    def test_fps_observations_are_recorded_without_capturing_pixels(self) -> None:
        program = (
            "print('SESSION fps 0 none', flush=True)\n"
            "print('SESSION fps 30 59.94fps', flush=True)\n"
            "print('SESSION fps 60 60.00fps', flush=True)\n"
            "print('SESSION end 90', flush=True)\n"
        )
        with tempfile.TemporaryDirectory() as folder:
            result = SESSION.capture(
                [sys.executable, "-c", program], Path(folder) / "out", 5
            )
            self.assertEqual(
                result["fps"],
                [
                    {"frame": 0, "value": "none"},
                    {"frame": 30, "value": "59.94"},
                    {"frame": 60, "value": "60.00"},
                ],
            )
            self.assertEqual(result["captures"], [])

    def test_invalid_fps_timelines_fail(self) -> None:
        cases = [
            ["SESSION fps 0 nanfps"],
            ["SESSION fps 0 -1.00fps"],
            ["SESSION fps 0 60.00fps", "SESSION fps 0 59.94fps"],
            ["SESSION fps 0 60.00fps", "SESSION fps 1 none"],
            ["SESSION fps 3 60.00fps"],
        ]
        for lines in cases:
            program = "\n".join(
                f"print({line!r}, flush=True)" for line in [*lines, "SESSION end 3"]
            )
            with self.subTest(lines=lines), tempfile.TemporaryDirectory() as folder:
                with self.assertRaises(ValueError):
                    SESSION.capture(
                        [sys.executable, "-c", program], Path(folder) / "out", 5
                    )

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
                image = fake_image()
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

    @unittest.skipUnless(os.name == "posix", "requires POSIX process groups")
    def test_a_dead_launcher_does_not_leave_its_child_running(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            child_pid = root / "child.pid"
            ready = root / "ready"
            terminated = root / "terminated"
            child = (
                "import pathlib, signal, sys, time\n"
                "def terminate(signum, frame):\n"
                f"    pathlib.Path({str(terminated)!r}).touch()\n"
                "    sys.exit(0)\n"
                "signal.signal(signal.SIGTERM, terminate)\n"
                f"pathlib.Path({str(ready)!r}).touch()\n"
                "time.sleep(60)\n"
            )
            program = (
                "import pathlib, subprocess, sys, time\n"
                f"child = subprocess.Popen([sys.executable, '-c', {child!r}])\n"
                f"pathlib.Path({str(child_pid)!r}).write_text(str(child.pid))\n"
                f"while not pathlib.Path({str(ready)!r}).exists(): time.sleep(.01)\n"
            )
            pid = None
            try:
                with self.assertRaises(TimeoutError):
                    SESSION.capture([sys.executable, "-c", program], root / "out", 2)
                pid = int(child_pid.read_text())
                self.assertTrue(terminated.exists(), "capture left its child running")
            finally:
                if pid is not None:
                    with contextlib.suppress(ProcessLookupError):
                        os.kill(pid, signal.SIGKILL)

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
