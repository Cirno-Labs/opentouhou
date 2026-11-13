#!/usr/bin/env python3
"""Run a test program that must stop with a fatal error.

Passes when the program aborts and its fatal log line contains the
expected text. Fails when it exits normally, crashes in any other way,
or reports a different fatal error.
"""

import signal
import subprocess
import sys

if sys.platform != "win32":
    import resource

# Holds the exit statuses of an aborted program: killed by SIGABRT on
# POSIX, and status 3 from the C runtime's abort() on Windows. A Windows
# program under wine also exits with status 3.
ABORT_STATUSES = [-signal.SIGABRT, 3]


def disable_core_dumps() -> None:
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def main() -> int:
    if len(sys.argv) < 3:
        sys.stderr.write(f"Usage: {sys.argv[0]} EXPECTED PROGRAM [ARGS...]\n")
        return 2

    expected = sys.argv[1]
    result = subprocess.run(
        sys.argv[2:],
        capture_output=True,
        text=True,
        check=False,
        preexec_fn=None if sys.platform == "win32" else disable_core_dumps,
    )
    fatal_lines = [
        line for line in result.stderr.splitlines() if line.startswith("FATAL ")
    ]

    errors = []
    if result.returncode not in ABORT_STATUSES:
        errors.append(
            f"expected the program to abort, got exit status {result.returncode}"
        )
    if not any(expected in line for line in fatal_lines):
        errors.append(f"no FATAL line contains {expected!r}")

    if errors:
        for error in errors:
            sys.stderr.write(f"{error}\n")
        sys.stderr.write("--- stdout\n" + result.stdout)
        sys.stderr.write("--- stderr\n" + result.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
