"""Check portable cases using synthetic state, input and screenshot bytes."""

from __future__ import annotations

import io
import json
import stat
import struct
import sys
import tempfile
import unittest
from collections.abc import Callable
from pathlib import Path
from unittest.mock import patch
from zipfile import ZipFile, ZipInfo

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

from shared.replay_case import CaseArchive, import_legacy, legacy_header, write_case


class CaseTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.inputs = self.root / "inputs.txt"
        self.inputs.write_text(
            "OTINPUT 1\nframes 10\ninput 2 shoot\ncapture 9\n", encoding="utf-8"
        )
        self.state = self.root / "state.otrc"
        self.state.write_bytes(b"OTRC\x01\x00" + bytes(range(256)) * 1024)
        self.image = self.root / "frame.png"
        self.image.write_bytes(b"synthetic PNG bytes")
        self.case = self.root / "test.otrc"

    def write(self, **options: object) -> None:
        values = dict(
            game="th07",
            inputs=self.inputs,
            input_kind="script",
            states=[self.state],
            screenshots={9: self.image},
        )
        values.update(options)
        write_case(self.case, **values)

    def rewrite(
        self, change: Callable[[dict[str, object], dict[str, bytes]], None]
    ) -> None:
        with ZipFile(self.case) as archive:
            entries = {name: archive.read(name) for name in archive.namelist()}
        manifest = json.loads(entries["manifest.json"])
        change(manifest, entries)
        entries["manifest.json"] = (json.dumps(manifest) + "\n").encode()
        with ZipFile(self.case, "w") as archive:
            for name, data in entries.items():
                archive.writestr(name, data)

    def test_round_trip_preserves_binary_bits_and_readable_inputs(self) -> None:
        self.write(metadata={"source": "synthetic", "label": "十"})
        with CaseArchive(self.case) as case:
            case.verify()
            self.assertEqual(case.manifest["game"], "th07")
            self.assertEqual(case.manifest["metadata"]["label"], "十")
            case.extract(self.root / "unpacked")
        self.assertEqual(
            (self.root / "unpacked/inputs.txt").read_bytes(), self.inputs.read_bytes()
        )
        self.assertEqual(
            (self.root / "unpacked/states/0000.otrc").read_bytes(),
            self.state.read_bytes(),
        )
        self.assertEqual(
            (self.root / "unpacked/screenshots/frame-00000009.png").read_bytes(),
            self.image.read_bytes(),
        )
        with ZipFile(self.case) as archive:
            self.assertIn("input 2 shoot", archive.read("inputs.txt").decode())

    def test_legacy_replay_and_trace_can_be_portable_without_a_new_capture(
        self,
    ) -> None:
        replay = self.root / "original.rpy"
        replay.write_bytes(b"synthetic replay\0\xff")
        self.write(inputs=replay, input_kind="replay")
        with CaseArchive(self.case) as case:
            case.verify()
            case.extract(self.root / "unpacked")
            self.assertEqual(
                case.manifest["inputs"], {"kind": "replay", "path": "replay.rpy"}
            )
        self.assertEqual(
            (self.root / "unpacked/replay.rpy").read_bytes(), replay.read_bytes()
        )

    def test_scripted_case_can_stage_a_replay_without_using_it_as_the_input_script(
        self,
    ) -> None:
        replay = self.root / "stage.rpy"
        replay.write_bytes(b"synthetic staged replay")
        self.write(staged_replay=replay)
        with CaseArchive(self.case) as case:
            case.verify()
            self.assertEqual(case.manifest["inputs"]["path"], "inputs.txt")
            self.assertEqual(case.manifest["staged_replay"], "replay.rpy")

    def test_does_not_replace_an_existing_case(self) -> None:
        self.case.write_bytes(b"existing case")
        with self.assertRaises(FileExistsError):
            self.write()
        self.assertEqual(self.case.read_bytes(), b"existing case")

    def test_failure_removes_only_the_output_it_created(self) -> None:
        with patch(
            "shared.replay_case.ZipFile.open", side_effect=OSError("write failed")
        ):
            with self.assertRaisesRegex(OSError, "write failed"):
                self.write()
        self.assertFalse(self.case.exists())
        self.assertTrue(self.state.exists())

    def test_rejects_a_changed_payload_even_with_a_valid_zip_crc(self) -> None:
        self.write()

        def change(manifest: dict[str, object], entries: dict[str, bytes]) -> None:
            entries["states/0000.otrc"] = b"X" * len(entries["states/0000.otrc"])

        self.rewrite(change)
        with CaseArchive(self.case) as case:
            with self.assertRaisesRegex(ValueError, "checksum"):
                case.verify()

    def test_rejects_path_traversal_and_windows_paths(self) -> None:
        for name in (
            ".",
            "../outside",
            "/outside",
            "C:/outside",
            "a/../outside",
            "a\\outside",
            "a//outside",
        ):
            with self.subTest(name=name):
                with ZipFile(self.case, "w") as archive:
                    archive.writestr(name, b"payload")
                with self.assertRaisesRegex(ValueError, "member"):
                    CaseArchive(self.case)
        self.assertFalse((self.root.parent / "outside").exists())

    def test_rejects_symlinks(self) -> None:
        with ZipFile(self.case, "w") as archive:
            info = ZipInfo("link")
            info.create_system = 3
            info.external_attr = (stat.S_IFLNK | 0o777) << 16
            archive.writestr(info, "outside")
        with self.assertRaisesRegex(ValueError, "regular"):
            CaseArchive(self.case)

    def test_manifest_must_declare_all_payloads(self) -> None:
        self.write()
        with ZipFile(self.case, "a") as archive:
            archive.writestr("extra", b"undeclared")
        with self.assertRaisesRegex(ValueError, "every payload"):
            CaseArchive(self.case)

    def test_rejects_untrusted_reference_types(self) -> None:
        self.write()

        def change(manifest: dict[str, object], entries: dict[str, bytes]) -> None:
            manifest["inputs"]["path"] = ["inputs.txt"]

        self.rewrite(change)
        with self.assertRaises(ValueError):
            CaseArchive(self.case)

    def test_can_verify_large_members_without_loading_the_whole_case(self) -> None:
        self.write()
        with CaseArchive(self.case) as case:
            output = io.BytesIO()
            case.copy_member("states/0000.otrc", output)
            self.assertEqual(output.getvalue(), self.state.read_bytes())

    def test_import_keeps_the_legacy_checkpoint_bits_and_input_bytes(self) -> None:
        name = b"seed"
        trace = (
            b"OTRC\x01\x00th07\0\0\0\0"
            + struct.pack("<I", 9)
            + b"synthetic"
            + struct.pack("<I", 1)
            + bytes([len(name)])
            + name
            + struct.pack("<II", 0x80000000, 1)
            + bytes([len(name)])
            + name
            + b"compressed rows stay untouched"
        )
        self.state.write_bytes(trace)
        replay = self.root / "test.rpy"
        replay.write_bytes(b"synthetic original-format input bytes")
        import_legacy(self.state, replay, self.case)
        with CaseArchive(self.case) as case:
            case.verify()
            self.assertEqual(case.manifest["metadata"]["start"], {"seed": 0x80000000})
            case.extract(self.root / "unpacked")
        self.assertEqual((self.root / "unpacked/states/0000.otrc").read_bytes(), trace)
        self.assertEqual(
            (self.root / "unpacked/replay.rpy").read_bytes(), replay.read_bytes()
        )

    def test_legacy_header_rejects_truncation_and_wrong_versions(self) -> None:
        for data in (b"", b"OTRC\x02\x00", b"OTRC\x01\x00th07"):
            with self.subTest(data=data), self.assertRaises(ValueError):
                legacy_header(io.BytesIO(data))


if __name__ == "__main__":
    unittest.main()
