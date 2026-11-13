"""Tests of tools/release_index and tools/release_plan against a fake R2
bucket."""

import contextlib
import datetime
import hashlib
import importlib.machinery
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from types import ModuleType
from unittest import mock

TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"
FAKE_R2 = Path(__file__).resolve().parent / "fake_r2.py"
PUBLIC_URL = "https://pub.example.com"
ENV = {
    "R2_ACCOUNT_ID": "account",
    "R2_BUCKET": "bucket",
    "R2_PUBLIC_URL": PUBLIC_URL + "/",
}

sys.path.insert(0, str(TOOLS_DIR))
sys.path.insert(0, str(FAKE_R2.parent))

from fake_r2 import FakeR2
from shared import r2, release_index

# Gives the changelogs of the repository that the tools run on.
CHANGELOGS = {
    "othpatch": """# othpatch changelog

## Unreleased

- Added reading the index.

## 0.1.2 (2026-11-13)

- Changed to ship with a game.

## 0.1.1 (2026-11-12)

- Changed nothing for players. This release checks that othpatch
  updates itself.

## 0.1.0 (2026-11-11)

- Added the first release.
""",
    "th06": """# th06 changelog

## 0.1.1 (2026-11-13)

- Changed the packages to ship othpatch 0.1.2.

## 0.1.0 (2026-11-12)

- Added the first release.
""",
}


def load_tool(name: str) -> ModuleType:
    """Load a tool under tools/, a script without .py, as a module."""
    loader = importlib.machinery.SourceFileLoader(name, str(TOOLS_DIR / name))
    module = importlib.util.module_from_spec(importlib.util.spec_from_loader(name, loader))
    loader.exec_module(module)
    return module


RELEASE_INDEX = load_tool("release_index")
RELEASE_PLAN = load_tool("release_plan")


def write_repo(repo: Path) -> None:
    """Write the changelogs and version files that the tools read."""
    for program, changelog in CHANGELOGS.items():
        (repo / "docs" / program).mkdir(parents=True)
        (repo / "docs" / program / "CHANGELOG.md").write_text(changelog)
        (repo / "src" / program).mkdir(parents=True)
        (repo / "src" / program / "version.txt").write_text("0.1.0\n")
    (repo / "src/th06/othpatch.txt").write_text("0.1.0\n")


def run_main(tool: ModuleType, args: list[str], repo: Path) -> subprocess.CompletedProcess:
    """Run the main function of a tool and return its outcome as a process
    would report it."""
    stdout, stderr = io.StringIO(), io.StringIO()
    code = 0
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        try:
            tool.main(args, repo)
        except SystemExit as error:
            code = error.code
    if code is None:
        code = 0
    elif isinstance(code, str):
        stderr.write(code + "\n")
        code = 1
    return subprocess.CompletedProcess(args, code, stdout.getvalue(), stderr.getvalue())


class ReleaseIndexTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.repo_dir = tempfile.TemporaryDirectory()
        cls.repo = Path(cls.repo_dir.name)
        write_repo(cls.repo)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.repo_dir.cleanup()

    def setUp(self) -> None:
        self.dir = tempfile.TemporaryDirectory()
        self.root = Path(self.dir.name)
        (self.root / "packages").mkdir()
        self.r2 = FakeR2()
        self.env = dict(ENV)

    def tearDown(self) -> None:
        self.dir.cleanup()

    def run_tool(self, *args: str, fail: str = "") -> subprocess.CompletedProcess:
        self.r2.fail = fail
        with mock.patch.dict(os.environ, self.env), mock.patch.object(r2, "run", self.r2.run):
            return run_main(RELEASE_INDEX, list(args), self.repo)

    def package(self, name: str, contents: bytes = b"contents") -> None:
        (self.root / "packages" / name).write_bytes(contents)

    def publish(self, program: str, version: str, *extra: str, fail: str = ""):
        return self.run_tool(
            "publish",
            "--program",
            program,
            "--version",
            version,
            "--packages",
            str(self.root / "packages"),
            *extra,
            fail=fail,
        )

    def index(self) -> dict:
        return json.loads(self.r2.objects["releases.json"])

    def folder(self, prefix: str) -> dict[str, bytes]:
        """Return the objects under a prefix, by the rest of their keys."""
        return {
            key.removeprefix(prefix): data
            for key, data in self.r2.objects.items()
            if key.startswith(prefix)
        }

    def test_publish_uploads_the_files_then_the_index(self) -> None:
        self.package("othpatch-0.1.1-Linux", b"linux")
        self.package("othpatch-0.1.1-Windows.exe")
        self.package("othpatch-0.1.0-macOS")
        result = self.publish("othpatch", "0.1.1")
        self.assertEqual(result.returncode, 0, result.stderr)
        release = self.index()["programs"]["othpatch"][0]
        self.assertEqual(release["version"], "0.1.1")
        self.assertEqual(release["date"], "2026-11-12")
        self.assertIn("othpatch\n  updates itself", release["changes"])
        self.assertNotIn("othpatch", release)
        self.assertEqual(sorted(release["files"]), ["Linux", "Windows"])
        linux = release["files"]["Linux"]
        self.assertEqual(
            linux,
            {
                "name": "othpatch-0.1.1-Linux",
                "url": f"{PUBLIC_URL}/othpatch/0.1.1/othpatch-0.1.1-Linux",
                "size": 5,
                "sha256": hashlib.sha256(b"linux").hexdigest(),
                "sha1": hashlib.sha1(b"linux").hexdigest(),
            },
        )
        self.assertEqual(
            release["checksums"],
            {
                "SHA256SUMS": f"{PUBLIC_URL}/othpatch/0.1.1/SHA256SUMS",
                "SHA1SUMS": f"{PUBLIC_URL}/othpatch/0.1.1/SHA1SUMS",
            },
        )
        self.assertIn("othpatch/0.1.1/othpatch-0.1.1-Linux", self.r2.objects)
        log = self.r2.log
        self.assertIn("s3 cp", log[-1])
        self.assertIn("s3://bucket/releases.json", log[-1])
        self.assertIn("--cache-control no-cache", log[-1])

    def test_checksum_files_check_the_downloads(self) -> None:
        self.package("othpatch-0.1.1-Windows.exe", b"windows")
        self.package("othpatch-0.1.1-Linux", b"linux")
        self.assertEqual(self.publish("othpatch", "0.1.1").returncode, 0)
        folder = self.root / "objects"
        folder.mkdir()
        for name, data in self.folder("othpatch/0.1.1/").items():
            (folder / name).write_bytes(data)
        self.assertEqual(
            (folder / "SHA256SUMS").read_text(),
            f"{hashlib.sha256(b'linux').hexdigest()}  othpatch-0.1.1-Linux\n"
            f"{hashlib.sha256(b'windows').hexdigest()}  othpatch-0.1.1-Windows.exe\n",
        )
        self.assertEqual(
            (folder / "SHA1SUMS").read_text().splitlines()[0],
            f"{hashlib.sha1(b'linux').hexdigest()}  othpatch-0.1.1-Linux",
        )
        for tool, name in [("sha256sum", "SHA256SUMS"), ("sha1sum", "SHA1SUMS")]:
            if shutil.which(tool) is not None:
                result = subprocess.run([tool, "-c", name], cwd=folder, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stdout)

    def test_releases_stay_newest_first(self) -> None:
        self.package("othpatch-0.1.1-Linux")
        self.package("othpatch-0.1.2-Linux")
        self.package("othpatch-0.1.0-Linux")
        for version in ["0.1.1", "0.1.2"]:
            self.assertEqual(self.publish("othpatch", version).returncode, 0)
        versions = [item["version"] for item in self.index()["programs"]["othpatch"]]
        self.assertEqual(versions, ["0.1.2", "0.1.1"])

    def test_published_version_is_never_replaced(self) -> None:
        self.package("othpatch-0.1.1-Linux")
        self.assertEqual(self.publish("othpatch", "0.1.1").returncode, 0)
        before = self.index()
        result = self.publish("othpatch", "0.1.1")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("published already", result.stderr)
        self.assertEqual(self.index(), before)

    def set_published(self, program: str, version: str, hours: float | None) -> None:
        index = self.index()
        release = next(
            item for item in index["programs"][program] if item["version"] == version
        )
        if hours is None:
            del release["published"]
        else:
            time = datetime.datetime.now(datetime.UTC) - datetime.timedelta(hours=hours)
            release["published"] = time.replace(microsecond=0).isoformat()
        self.r2.objects["releases.json"] = json.dumps(index).encode()

    def test_publish_records_the_time(self) -> None:
        self.package("othpatch-0.1.1-Linux")
        self.assertEqual(self.publish("othpatch", "0.1.1").returncode, 0)
        published = self.index()["programs"]["othpatch"][0]["published"]
        self.assertRegex(published, r"^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$")

    def test_replace_within_24_hours_replaces_the_files(self) -> None:
        self.package("othpatch-0.1.1-Linux", b"old")
        self.package("othpatch-0.1.1-macOS", b"old")
        self.assertEqual(self.publish("othpatch", "0.1.1").returncode, 0)
        self.package("othpatch-0.1.2-Linux")
        self.assertEqual(self.publish("othpatch", "0.1.2").returncode, 0)
        self.set_published("othpatch", "0.1.1", 23.5)
        published = self.index()["programs"]["othpatch"][1]["published"]
        (self.root / "packages" / "othpatch-0.1.1-macOS").unlink()
        self.package("othpatch-0.1.1-Linux", b"new")
        result = self.publish("othpatch", "0.1.1", "--replace")
        self.assertEqual(result.returncode, 0, result.stderr)
        releases = self.index()["programs"]["othpatch"]
        self.assertEqual([item["version"] for item in releases], ["0.1.2", "0.1.1"])
        release = releases[1]
        self.assertEqual(release["published"], published)
        self.assertEqual(sorted(release["files"]), ["Linux"])
        self.assertEqual(release["files"]["Linux"]["sha256"], hashlib.sha256(b"new").hexdigest())
        folder = self.folder("othpatch/0.1.1/")
        self.assertEqual(folder["othpatch-0.1.1-Linux"], b"new")
        self.assertEqual(sorted(folder), ["SHA1SUMS", "SHA256SUMS", "othpatch-0.1.1-Linux"])
        self.assertIn(hashlib.sha256(b"new").hexdigest(), folder["SHA256SUMS"].decode())

    def test_replace_after_24_hours_fails(self) -> None:
        self.package("othpatch-0.1.1-Linux")
        self.assertEqual(self.publish("othpatch", "0.1.1").returncode, 0)
        self.set_published("othpatch", "0.1.1", 24.1)
        before = self.index()
        result = self.publish("othpatch", "0.1.1", "--replace")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("24 hours or more ago", result.stderr)
        self.assertEqual(self.index(), before)

    def test_replace_without_a_publication_time_fails(self) -> None:
        self.package("othpatch-0.1.1-Linux")
        self.assertEqual(self.publish("othpatch", "0.1.1").returncode, 0)
        self.set_published("othpatch", "0.1.1", None)
        result = self.publish("othpatch", "0.1.1", "--replace")
        self.assertIn("no time of its publication", result.stderr)

    def test_replace_of_a_new_version_publishes_it(self) -> None:
        self.package("othpatch-0.1.1-Linux")
        result = self.publish("othpatch", "0.1.1", "--replace")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Published", result.stdout)

    def plan(self, index: dict, *extra: str) -> subprocess.CompletedProcess:
        path = self.root / "plan.json"
        path.write_text(json.dumps(index))
        return run_main(RELEASE_PLAN, ["--index", str(path), *extra], self.repo)

    def test_plan_replaces_only_releases_younger_than_24_hours(self) -> None:
        now = datetime.datetime.now(datetime.UTC)

        def release(hours: float) -> dict:
            time = now - datetime.timedelta(hours=hours)
            return {"version": "0.1.0", "published": time.isoformat()}

        index = {"format": 1, "programs": {"othpatch": [release(30)], "oth06": [release(2)]}}
        result = self.plan(index)
        self.assertEqual(result.stdout, "othpatch=\ngames=\n")
        result = self.plan(index, "--replace")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "othpatch=\ngames=th06\n")
        self.assertIn("othpatch 0.1.0 cannot be replaced", result.stderr)
        index["programs"]["oth06"] = [release(25)]
        result = self.plan(index, "--replace")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("th06 0.1.0 cannot be replaced", result.stderr)

    def test_game_release_records_its_othpatch(self) -> None:
        self.package("oth06-0.1.1-Linux.zip")
        self.assertEqual(self.publish("th06", "0.1.1", "--othpatch", "0.1.2").returncode, 0)
        release = self.index()["programs"]["oth06"][0]
        self.assertEqual(release["othpatch"], "0.1.2")
        self.assertEqual(release["files"]["Linux"]["name"], "oth06-0.1.1-Linux.zip")

    def test_game_release_has_the_product_name(self) -> None:
        self.package("oth06-0.1.1-Linux.zip")
        self.assertEqual(self.publish("th06", "0.1.1").returncode, 0)
        self.assertEqual(list(self.index()["programs"]), ["oth06"])
        release = self.index()["programs"]["oth06"][0]
        self.assertEqual(
            release["files"]["Linux"]["url"],
            f"{PUBLIC_URL}/oth06/0.1.1/oth06-0.1.1-Linux.zip",
        )
        self.assertEqual(
            release["checksums"]["SHA256SUMS"], f"{PUBLIC_URL}/oth06/0.1.1/SHA256SUMS"
        )
        self.assertIn("oth06/0.1.1/oth06-0.1.1-Linux.zip", self.r2.objects)
        self.assertEqual(self.folder("th06/"), {})
        self.assertEqual(self.run_tool("has", "th06", "0.1.1").returncode, 0)

    def test_tag_has_the_product_name(self) -> None:
        for program, tag in [("th06", "oth06-v1.2.0"), ("othpatch", "othpatch-v1.2.0")]:
            result = self.run_tool("tag", program, "1.2.0")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, f"{tag}\n")

    def test_failed_upload_leaves_the_index_alone(self) -> None:
        self.package("othpatch-0.1.1-Linux")
        result = self.publish("othpatch", "0.1.1", fail="s3 cp")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("fake failure", result.stderr)
        self.assertNotIn("releases.json", self.r2.objects)

    def test_failed_index_read_is_not_an_empty_index(self) -> None:
        self.package("othpatch-0.1.1-Linux")
        result = self.publish("othpatch", "0.1.1", fail="s3api get-object")
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("releases.json", self.r2.objects)

    def test_missing_public_url_fails_before_any_upload(self) -> None:
        self.package("othpatch-0.1.1-Linux")
        self.env["R2_PUBLIC_URL"] = ""
        result = self.publish("othpatch", "0.1.1")
        self.assertIn("R2_PUBLIC_URL", result.stderr)
        self.assertEqual(self.r2.log, [])

    def test_version_without_a_dated_heading_fails(self) -> None:
        self.package("othpatch-9.9.9-Linux")
        result = self.publish("othpatch", "9.9.9")
        self.assertIn("no heading", result.stderr)

    def test_fetch_and_has_read_the_index(self) -> None:
        out = self.root / "index.json"
        self.assertEqual(self.run_tool("fetch", str(out)).returncode, 0)
        self.assertEqual(json.loads(out.read_text()), {"format": 1, "programs": {}})
        self.package("othpatch-0.1.1-Linux")
        self.publish("othpatch", "0.1.1")
        self.assertEqual(self.run_tool("has", "othpatch", "0.1.1").returncode, 0)
        self.assertEqual(self.run_tool("has", "othpatch", "0.1.2").returncode, 1)

    def test_reset_deletes_the_releases_and_keeps_the_snapshot(self) -> None:
        self.package("othpatch-0.1.1-Linux")
        self.assertEqual(self.publish("othpatch", "0.1.1").returncode, 0)
        self.package("oth06-0.1.1-Linux.zip")
        self.assertEqual(self.publish("th06", "0.1.1").returncode, 0)
        self.r2.objects["oth06/0.1.0/oth06-0.1.0-Linux.zip"] = b""
        shutil.rmtree(self.root / "packages")
        (self.root / "packages").mkdir()
        self.package("oth06-0.1.2-dev-abcdef0-Linux.zip")
        result = self.run_tool(
            "snapshot", "--packages", str(self.root / "packages"), "--commit", "abcdef0123"
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        snapshot = self.index()["snapshot"]

        result = self.run_tool("reset")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.index(), {"format": 1, "programs": {}, "snapshot": snapshot})
        self.assertEqual(self.folder("othpatch/"), {})
        self.assertEqual(self.folder("oth06/"), {})
        self.assertIn("snapshot/oth06-0.1.2-dev-abcdef0-Linux.zip", self.r2.objects)
        log = self.r2.log
        puts = [i for i, line in enumerate(log) if line.startswith("s3 cp")]
        index_put = max(i for i in puts if "releases.json" in log[i])
        deletes = [i for i, line in enumerate(log) if line.startswith("s3 rm")]
        self.assertLess(index_put, min(deletes))

    def test_reset_of_an_empty_bucket_writes_an_empty_index(self) -> None:
        result = self.run_tool("reset")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.index(), {"format": 1, "programs": {}})

    def test_snapshot_replaces_the_older_snapshot(self) -> None:
        self.package("othpatch-0.1.1-Linux")
        self.publish("othpatch", "0.1.1")
        self.r2.objects["snapshot/oth06-0.1.0-dev-1111111-Linux.zip"] = b""
        self.package("oth06-0.1.2-dev-abcdef0-Linux.zip")
        self.package("oth06-0.1.2-dev-abcdef0-Windows.zip")
        result = self.run_tool(
            "snapshot", "--packages", str(self.root / "packages"), "--commit", "abcdef0123"
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        index = self.index()
        self.assertEqual(index["programs"]["othpatch"][0]["version"], "0.1.1")
        snapshot = index["snapshot"]
        self.assertEqual(snapshot["commit"], "abcdef0123")
        self.assertRegex(snapshot["date"], r"^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$")
        game = snapshot["programs"]["oth06"]
        self.assertEqual(game["version"], "0.1.2-dev-abcdef0")
        self.assertEqual(
            game["files"]["Windows"]["url"],
            f"{PUBLIC_URL}/snapshot/oth06-0.1.2-dev-abcdef0-Windows.zip",
        )
        self.assertIn("othpatch", snapshot["programs"])
        self.assertEqual(
            sorted(self.folder("snapshot/")),
            [
                "SHA1SUMS",
                "SHA256SUMS",
                "oth06-0.1.2-dev-abcdef0-Linux.zip",
                "oth06-0.1.2-dev-abcdef0-Windows.zip",
                "othpatch-0.1.1-Linux",
            ],
        )
        sums = self.r2.objects["snapshot/SHA256SUMS"].decode()
        self.assertEqual(len(sums.splitlines()), 3)
        self.assertEqual(
            snapshot["checksums"]["SHA1SUMS"], f"{PUBLIC_URL}/snapshot/SHA1SUMS"
        )


class CommandLineTest(unittest.TestCase):
    """Runs the tools as programs, with the fake bucket as the aws command."""

    def test_publish_uploads_through_the_aws_command(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            repo = root / "repo"
            write_repo(repo)
            shutil.copytree(
                TOOLS_DIR / "shared",
                repo / "tools/shared",
                ignore=shutil.ignore_patterns("__pycache__"),
            )
            shutil.copy2(TOOLS_DIR / "release_index", repo / "tools")
            (root / "bin").mkdir()
            (root / "objects").mkdir()
            aws = root / "bin" / "aws"
            aws.write_text(
                f"#!{sys.executable}\nimport runpy\n"
                f"runpy.run_path({str(FAKE_R2)!r}, run_name='__main__')\n"
            )
            aws.chmod(0o755)
            (root / "packages").mkdir()
            (root / "packages/othpatch-0.1.1-Linux").write_bytes(b"linux")
            env = dict(
                os.environ,
                **ENV,
                PATH=f"{root / 'bin'}{os.pathsep}{os.environ['PATH']}",
                FAKE_R2=str(root),
            )
            tool = [sys.executable, str(repo / "tools/release_index")]
            result = subprocess.run(
                [*tool, "publish", "--program", "othpatch", "--version", "0.1.1"]
                + ["--packages", str(root / "packages")],
                env=env,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("Published othpatch 0.1.1", result.stdout)
            index = json.loads((root / "objects/releases.json").read_text())
            self.assertEqual(index["programs"]["othpatch"][0]["version"], "0.1.1")
            self.assertEqual(
                (root / "objects/othpatch/0.1.1/othpatch-0.1.1-Linux").read_bytes(), b"linux"
            )
            log = (root / "log").read_text().splitlines()
            self.assertTrue(log[0].startswith("s3api get-object --bucket bucket"))
            self.assertIn("--endpoint-url https://account.r2.cloudflarestorage.com", log[0])


class ReplaceRuleTest(unittest.TestCase):
    NOW = datetime.datetime(2026, 11, 12, 12, 0, tzinfo=datetime.UTC)

    def error(self, release: dict) -> str | None:
        return release_index.replace_error("th06", {"version": "1.0.0", **release}, self.NOW)

    def test_release_younger_than_24_hours_can_be_replaced(self) -> None:
        self.assertIsNone(self.error({"published": "2026-11-12T11:59:59Z"}))
        self.assertIsNone(self.error({"published": "2026-11-11T12:00:01Z"}))

    def test_release_of_24_hours_or_more_cannot_be_replaced(self) -> None:
        self.assertIn("24 hours or more", self.error({"published": "2026-11-11T12:00:00Z"}))
        self.assertIn("24 hours or more", self.error({"published": "2026-10-01T00:00:00Z"}))

    def test_release_without_a_time_cannot_be_replaced(self) -> None:
        self.assertIn("no time", self.error({"date": "2026-11-12"}))

    def test_replaced_release_keeps_its_place(self) -> None:
        index = release_index.empty()
        for version in ["1.0.0", "1.1.0"]:
            release_index.add_release(index, "th06", {"version": version})
        release_index.remove_release(index, "th06", "1.0.0")
        release_index.add_release(index, "th06", {"version": "1.0.0", "new": True})
        releases = index["programs"]["oth06"]
        self.assertEqual([item["version"] for item in releases], ["1.1.0", "1.0.0"])
        self.assertTrue(releases[1]["new"])


if __name__ == "__main__":
    unittest.main()
