"""Read and write the files of the R2 bucket through the AWS CLI.

The environment gives the bucket: R2_ACCOUNT_ID and R2_BUCKET name it,
and AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY hold a key that can read
and write it. Each function raises RuntimeError with the AWS CLI's
message when a command fails.
"""

import json
import os
import subprocess
from pathlib import Path


def require(name: str) -> str:
    """Return the value of an environment variable.

    Raises RuntimeError when it is empty or not set.
    """
    value = os.environ.get(name, "")
    if not value:
        raise RuntimeError(f"The environment variable {name} is not set")
    return value


def bucket() -> str:
    return require("R2_BUCKET")


def run(command: list[str], env: dict[str, str]) -> subprocess.CompletedProcess:
    """Run an AWS CLI command line and return its outcome.

    The tests replace this function with a fake bucket in memory.
    """
    return subprocess.run(command, capture_output=True, text=True, env=env)


def aws(*args: str) -> subprocess.CompletedProcess:
    """Run an AWS CLI command against the R2 endpoint and return its
    outcome without checking it."""
    endpoint = f"https://{require('R2_ACCOUNT_ID')}.r2.cloudflarestorage.com"
    env = dict(os.environ, AWS_DEFAULT_REGION="auto", AWS_PAGER="")
    return run(["aws", *args, "--endpoint-url", endpoint], env)


def check(result: subprocess.CompletedProcess, what: str) -> str:
    if result.returncode != 0:
        raise RuntimeError(f"Cannot {what}: {result.stderr.strip()}")
    return result.stdout


def get(key: str, path: Path) -> bool:
    """Download an object into a file. Returns whether the object exists."""
    result = aws("s3api", "get-object", "--bucket", bucket(), "--key", key, str(path))
    if result.returncode != 0 and "NoSuchKey" in result.stderr:
        return False
    check(result, f"download {key}")
    return True


def put(path: Path, key: str, content_type: str, cache_control: str | None = None) -> None:
    """Upload a file as an object, replacing the object of that key."""
    args = ["s3", "cp", str(path), f"s3://{bucket()}/{key}", "--content-type", content_type]
    if cache_control is not None:
        args += ["--cache-control", cache_control]
    check(aws(*args), f"upload {key}")


def keys(prefix: str) -> list[str]:
    """Return the keys of the objects whose keys start with prefix."""
    output = check(
        aws(
            "s3api",
            "list-objects-v2",
            "--bucket",
            bucket(),
            "--prefix",
            prefix,
            "--query",
            "Contents[].Key",
            "--output",
            "json",
        ),
        f"list {prefix}",
    )
    return json.loads(output or "null") or []


def delete(key: str) -> None:
    """Delete an object."""
    check(aws("s3", "rm", f"s3://{bucket()}/{key}"), f"delete {key}")
