"""Stand in for the R2 bucket that tools/shared/r2.py reads and writes.

FakeR2 keeps the objects in memory and answers the AWS CLI commands that
r2.py runs. Its run method replaces r2.run. Each command line goes into
log. A command whose first two words are in fail, as "s3 cp", fails.

Run as a program, this file stands in for the aws command itself. It
keeps the objects as files under $FAKE_R2/objects/, appends each command
line to $FAKE_R2/log and takes fail from $FAKE_R2_FAIL.
"""

import json
import os
import subprocess
import sys
from pathlib import Path


class FakeR2:
    def __init__(self) -> None:
        self.objects: dict[str, bytes] = {}
        self.log: list[str] = []
        self.fail = ""

    def run(
        self, command: list[str], env: dict[str, str]
    ) -> subprocess.CompletedProcess:
        code, stdout, stderr = self.aws(command[1:])
        return subprocess.CompletedProcess(command, code, stdout, stderr)

    def aws(self, args: list[str]) -> tuple[int, str, str]:
        """Run the command line of an aws command without the word aws, and
        return its exit status, output and error output."""
        self.log.append(" ".join(args))
        command = " ".join(args[:2])
        if self.fail and command in self.fail:
            return 1, "", "An error occurred (InternalError): fake failure"

        def option(name: str) -> str:
            return args[args.index(name) + 1]

        def key_of(url: str) -> str:
            return url.removeprefix("s3://").partition("/")[2]

        if command == "s3api get-object":
            key = option("--key")
            if key not in self.objects:
                return 1, "", "An error occurred (NoSuchKey) when calling GetObject"
            Path(args[args.index("--key") + 2]).write_bytes(self.objects[key])
        elif command == "s3 cp":
            self.objects[key_of(args[3])] = Path(args[2]).read_bytes()
        elif command == "s3api list-objects-v2":
            prefix = option("--prefix")
            found = sorted(key for key in self.objects if key.startswith(prefix))
            return 0, json.dumps(found or None), ""
        elif command == "s3 rm":
            del self.objects[key_of(args[2])]
        else:
            return 1, "", f"fake aws: unknown command {args}"
        return 0, "", ""


def main() -> int:
    root = Path(os.environ["FAKE_R2"])
    folder = root / "objects"
    fake = FakeR2()
    fake.fail = os.environ.get("FAKE_R2_FAIL", "")
    for path in folder.rglob("*"):
        if path.is_file():
            fake.objects[path.relative_to(folder).as_posix()] = path.read_bytes()
    before = set(fake.objects)
    code, stdout, stderr = fake.aws(sys.argv[1:])
    for key in before - set(fake.objects):
        (folder / key).unlink()
    for key, data in fake.objects.items():
        (folder / key).parent.mkdir(parents=True, exist_ok=True)
        (folder / key).write_bytes(data)
    with (root / "log").open("a") as log:
        log.write(fake.log[0] + "\n")
    sys.stdout.write(stdout)
    sys.stderr.write(stderr)
    return code


if __name__ == "__main__":
    sys.exit(main())
