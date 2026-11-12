"""Stand in for the AWS CLI commands that tools/shared/r2.py runs.

Keeps the objects as files under $FAKE_R2/objects/ and appends each
command line to $FAKE_R2/log. A command whose first two words are in
$FAKE_R2_FAIL, as "s3 cp", fails.
"""

import json
import os
import shutil
import sys
from pathlib import Path

root = Path(os.environ["FAKE_R2"])
objects = root / "objects"
args = sys.argv[1:]
with (root / "log").open("a") as log:
    log.write(" ".join(args) + "\n")
if " ".join(args[:2]) in os.environ.get("FAKE_R2_FAIL", ""):
    sys.exit("An error occurred (InternalError): fake failure")


def option(name: str) -> str:
    return args[args.index(name) + 1]


def key_of(url: str) -> str:
    return url.removeprefix("s3://").partition("/")[2]


if args[:2] == ["s3api", "get-object"]:
    path = objects / option("--key")
    if not path.exists():
        sys.exit("An error occurred (NoSuchKey) when calling the GetObject operation")
    shutil.copy(path, args[args.index("--key") + 2])
elif args[:2] == ["s3", "cp"]:
    path = objects / key_of(args[3])
    path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy(args[2], path)
elif args[:2] == ["s3api", "list-objects-v2"]:
    prefix = option("--prefix")
    found = sorted(
        str(path.relative_to(objects))
        for path in objects.rglob("*")
        if path.is_file() and str(path.relative_to(objects)).startswith(prefix)
    )
    print(json.dumps(found or None))
elif args[:2] == ["s3", "rm"]:
    (objects / key_of(args[2])).unlink()
else:
    sys.exit(f"fake aws: unknown command {args}")
