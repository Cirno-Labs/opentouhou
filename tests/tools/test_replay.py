"""Exercise portable capture and playback with a synthetic backend."""

from __future__ import annotations

import contextlib
import io
import os
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from shared import replay
from shared.replay_case import CaseArchive


def trace() -> bytes:
    name = b"session_frame"
    return (
        b"OTRC\x01\x00th06\0\0\0\0"
        + struct.pack("<III", 0, 0, 1)
        + bytes([len(name)])
        + name
        + zlib.compress(struct.pack("<III", 0, 1, 2))
    )


class ReplayTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.inputs = self.root / "inputs.txt"
        self.inputs.write_text("OTINPUT 1\nframes 3\ncapture 2\n", encoding="utf-8")
        self.case = self.root / "reference.otrc"
        self.backend = self.root / "fake.py"
        self.backend.write_text(
            "import pathlib, sys\n"
            "args = sys.argv[1:]\n"
            "out = pathlib.Path(args[args.index('--out') + 1])\n"
            "out.mkdir(parents=True)\n"
            f"trace = {trace()!r}\n"
            "if '--compare-state' in args:\n"
            "    path = pathlib.Path(args[args.index('--compare-state') + 1])\n"
            "    assert path.read_bytes() == trace, 'state differs'\n"
            "(out / 'session.otrc').write_bytes(trace)\n"
            "if '--headless' not in args:\n"
            "    print('SESSION capture 2 0 0 640 480', flush=True)\n"
            "    assert input() == 'CAPTURED 2'\n"
            "print('SESSION end 3', flush=True)\n",
            encoding="utf-8",
        )
        program = self.root / ("th06replay.exe" if os.name == "nt" else "th06replay")
        program.touch()
        image = Mock()
        image.size = (640, 480)
        image.tobytes.return_value = b"synthetic RGB"
        image.save.side_effect = lambda path: path.write_bytes(b"synthetic PNG")
        # Keep the backend a Python script on platforms that need PE executables.
        real_popen = subprocess.Popen

        def popen(argv: list[str], **kwargs: object) -> subprocess.Popen[str]:
            return real_popen([sys.executable, str(self.backend), *argv[1:]], **kwargs)

        patcher = patch("shared.replay_session.subprocess.Popen", side_effect=popen)
        patcher.start()
        self.addCleanup(patcher.stop)
        grabber = patch("shared.replay_session.grab_window", return_value=image)
        grabber.start()
        self.addCleanup(grabber.stop)
        environment = patch.dict(os.environ, {"DISPLAY": ":synthetic"})
        environment.start()
        self.addCleanup(environment.stop)

    def invoke(self, *args: str) -> int:
        with contextlib.redirect_stdout(io.StringIO()):
            return replay.main(list(args))

    def record(self, *options: str) -> int:
        return self.invoke(
            "capture",
            "--backend",
            "native",
            "--game",
            "th06",
            "--inputs",
            str(self.inputs),
            "--data",
            str(self.root),
            "--build-dir",
            str(self.root),
            "--out",
            str(self.case),
            *options,
        )

    def test_recorded_fps_is_inside_the_portable_script(self) -> None:
        text = self.backend.read_text(encoding="utf-8")
        marker = "if '--headless' not in args:"
        text = text.replace(
            marker,
            "if '--compare-state' not in args:\n"
            "    print('SESSION fps 0 none', flush=True)\n"
            "    print('SESSION fps 1 59.94fps', flush=True)\n" + marker,
        )
        self.backend.write_text(text, encoding="utf-8")
        self.assertEqual(self.record("--fast"), 0)
        with CaseArchive(self.case) as archive:
            archive.verify()
            with archive.member("inputs.txt") as stream:
                recorded = stream.read().decode("utf-8")
            self.assertIn("fps 0 none\nfps 1 59.94\n", recorded)
        self.assertNotIn("fps ", self.inputs.read_text(encoding="utf-8"))
        self.assertEqual(
            self.invoke(
                "run",
                str(self.case),
                "--data",
                str(self.root),
                "--build-dir",
                str(self.root),
                "--headless",
            ),
            0,
        )

    def test_capture_rejects_existing_tab_separated_fps_observations(self) -> None:
        self.inputs.write_text("OTINPUT 1\nframes 3\nfps\t0\t60.00\n", encoding="utf-8")
        self.backend.write_text(
            "print('SESSION fps 0 none', flush=True)\n"
            + self.backend.read_text(encoding="utf-8"),
            encoding="utf-8",
        )
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(self.record("--headless"), 1)
        self.assertFalse(self.case.exists())

    def test_one_file_round_trip_includes_state_inputs_and_screenshots(self) -> None:
        self.assertEqual(self.record("--fast"), 0)
        with CaseArchive(self.case) as archive:
            archive.verify()
            self.assertEqual(len(archive.manifest["screenshots"]), 1)
            with archive.member("inputs.txt") as stream:
                self.assertEqual(stream.read(), self.inputs.read_bytes())
        actual = self.root / "actual.otrc"
        self.assertEqual(
            self.invoke(
                "run",
                str(self.case),
                "--data",
                str(self.root),
                "--build-dir",
                str(self.root),
                "--headless",
                "--fast",
                "--out",
                str(actual),
            ),
            0,
        )
        with CaseArchive(actual) as archive:
            archive.verify()
            self.assertEqual(archive.manifest["screenshots"], [])
            with archive.member("inputs.txt") as stream:
                self.assertIn(b"capture 2", stream.read())
        with contextlib.redirect_stdout(io.StringIO()) as text:
            self.assertEqual(replay.main(["inspect", str(self.case), "--inputs"]), 0)
        self.assertEqual(text.getvalue(), self.inputs.read_text())

    def test_headless_capture_retains_the_screenshot_timeline(self) -> None:
        self.assertEqual(self.record("--headless", "--fast"), 0)
        with CaseArchive(self.case) as archive:
            self.assertEqual(archive.manifest["screenshots"], [])
            with archive.member("inputs.txt") as stream:
                self.assertIn(b"capture 2", stream.read())

    def test_rejects_overwrite_and_does_not_replace_existing_bytes(self) -> None:
        self.case.write_bytes(b"keep")
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(self.record(), 1)
        self.assertEqual(self.case.read_bytes(), b"keep")

    def test_backend_failure_does_not_publish_a_case(self) -> None:
        self.backend.write_text("raise SystemExit(3)\n", encoding="utf-8")
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(self.record("--headless"), 1)
        self.assertFalse(self.case.exists())

    def test_wine_paths_are_absolute_and_work_directory_is_explicit(self) -> None:
        program = self.root / "th06/th06capture.exe"
        program.parent.mkdir()
        program.touch()
        args = replay.parser().parse_args(
            [
                "capture",
                "--game",
                "th06",
                "--data",
                str(self.root),
                "--inputs",
                str(self.inputs),
                "--out",
                str(self.case),
                "--retail-dir",
                str(self.root),
                "--headless",
                "--fast",
            ]
        )
        work = self.root / "output/game"
        with patch("shared.replay.shutil.which", return_value="available"):
            argv, env = replay.command(args, "th06", self.inputs, None, work)
        self.assertEqual(argv[argv.index("--out") + 1], replay.retail_path(work))
        self.assertEqual(
            argv[argv.index("--inputs") + 1], replay.retail_path(self.inputs)
        )
        self.assertIn("--headless", argv)
        self.assertIn("--fast", argv)
        if os.name != "nt":
            self.assertIn("WINEPREFIX", env)


if __name__ == "__main__":
    unittest.main()
