"""Tests of tools/harness and tools/stage_rpy without a game."""

import importlib.machinery
import importlib.util
import tempfile
import unittest
from pathlib import Path
from types import ModuleType
from unittest import mock

TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"


def load(name: str) -> ModuleType:
    loader = importlib.machinery.SourceFileLoader(name, str(TOOLS_DIR / name))
    spec = importlib.util.spec_from_loader(name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


harness = load("harness")
stage_rpy = load("stage_rpy")


class HarnessTest(unittest.TestCase):
    def setUp(self) -> None:
        # resolve: on macOS the temporary folder lies behind a symlink.
        self.dir = Path(tempfile.mkdtemp()).resolve()

    def test_a_capture_passes_the_command_line_to_the_program(self) -> None:
        commands = []

        def run(command, env=None):
            commands.append(command)
            return mock.Mock(returncode=0)

        script = self.dir / "inputs.txt"
        script.write_text("frames 1\n")
        rpy = self.dir / "x.rpy"
        program = self.dir / "th06native"
        program.touch()
        argv = ["capture", "th06", str(self.dir / "c.othtc"), "--data"]
        argv += [str(self.dir), "--script", str(script), "--headless"]
        argv += ["--file", f"{rpy}=replay/a.rpy", "--program", str(program)]
        with mock.patch.object(harness.subprocess, "run", run):
            self.assertEqual(harness.main(argv), 0)
        command = commands[0]
        self.assertEqual(command[0], str(program))
        self.assertEqual(command[1:3], ["capture", str(self.dir / "c.othtc")])
        self.assertIn("--headless", command)
        self.assertEqual(command[command.index("--script") + 1], str(script))
        self.assertEqual(command[command.index("--file") + 1], f"{rpy}=replay/a.rpy")

    def test_a_dosbox_run_starts_a_container_with_its_paths(self) -> None:
        commands = []

        def run(command, env=None):
            commands.append(command)
            return mock.Mock(returncode=0)

        case = self.dir / "cases" / "c.othtc"
        case.parent.mkdir()
        case.touch()
        data = self.dir / "data"
        data.mkdir()
        out = self.dir / "out"
        program = self.dir / "bin" / "th01dosbox"
        program.parent.mkdir()
        program.touch()
        argv = ["replay", str(case), "--backend", "dosbox", "--data", str(data)]
        argv += ["--out", str(out), "--program", str(program)]
        with (
            mock.patch.object(harness.subprocess, "run", run),
            mock.patch.object(harness, "read_manifest") as manifest,
            mock.patch.object(harness.shutil, "which", return_value=None),
        ):
            manifest.return_value = {"case": {"game": "th01"}}
            self.assertEqual(harness.main(argv), 0)
        command = commands[0]
        self.assertEqual(command[:4], ["docker", "run", "--rm", "--init"])
        self.assertIn(f"{data}:{data}:ro", command)
        self.assertIn(f"{out}:{out}", command)
        self.assertIn(f"{case.parent}:{case.parent}:ro", command)
        self.assertIn(f"{program.parent}:{program.parent}:ro", command)
        image = command.index(harness.DOSBOX_IMAGE)
        self.assertEqual(command[image + 1 : image + 3], [str(program), "replay"])
        self.assertTrue(out.is_dir())

    def test_dosbox_takes_no_live_capture(self) -> None:
        argv = ["capture", "th01", str(self.dir / "c.othtc"), "--data"]
        argv += [str(self.dir), "--live", "--backend", "dosbox"]
        with self.assertRaises(SystemExit):
            harness.main(argv)

    def test_window_needs_the_retail_backend(self) -> None:
        argv = ["capture", "th06", str(self.dir / "c.othtc"), "--data"]
        argv += [str(self.dir), "--live", "--window"]
        with self.assertRaises(SystemExit):
            harness.main(argv)

    def test_stage_rpy_stages_into_the_games_slot(self) -> None:
        with mock.patch.object(stage_rpy.subprocess, "run") as run:
            run.return_value.returncode = 0
            stage_rpy.main(["th06", "x.rpy", "c.othtc", "--headless"])
        command = run.call_args.args[0]
        self.assertEqual(command[1:4], ["capture", "th06", "c.othtc"])
        self.assertIn("x.rpy=replay/th6_01.rpy", command)
        self.assertEqual(command[-1], "--headless")


if __name__ == "__main__":
    unittest.main()
