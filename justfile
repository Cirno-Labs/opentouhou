CWD := `pwd`
HOST_USER_UID := `id -u`
HOST_USER_GID := `id -g`

import "tools/docker/docker.just"

mod replay

# List the recipes.
[group('misc')]
default:
    @just --list

# Build the games in the media image and othpatch in the othpatch image.
[group('build')]
_build platform image target: (_docker_build platform image) (_docker_run platform image "build" "--target" target)

# Build every game and othpatch for Linux (debug or release).
[group('build')]
build-linux target="debug": (_build "linux" "media" target) (_build "linux" "othpatch" target)

# Build every game and othpatch for Windows (debug or release).
[group('build')]
build-win target="debug": (_build "win" "media" target) (_build "win" "othpatch" target)

# The othpatch of the commit is built too, for the packages to carry when
# its version has no release.
[doc("Package every game for Linux into build/packages/ (debug or release).")]
[group('build')]
package-linux target="debug": (_build "linux" "othpatch" target) (_build "linux" "media" target) (_docker_run "linux" "media" "package" "--target" target)

# Package every game for Windows into build/packages/ (debug or release).
[group('build')]
package-win target="debug": (_build "win" "othpatch" target) (_build "win" "media" target) (_docker_run "win" "media" "package" "--target" target)

# Write othpatch for Linux into build/packages/ (debug or release).
[group('build')]
package-othpatch-linux target="debug": (_build "linux" "othpatch" target) (_docker_run "linux" "othpatch" "package-othpatch" "--target" target)

# Write othpatch for Windows into build/packages/ (debug or release).
[group('build')]
package-othpatch-win target="debug": (_build "win" "othpatch" target) (_docker_run "win" "othpatch" "package-othpatch" "--target" target)

# Build the macOS dependencies into build/mac-deps (on a Mac).
[group('build')]
deps-mac:
    tools/mac/build deps

# Build every game and othpatch for macOS, arm64 and x86-64 (debug or release).
[group('build')]
build-mac target="debug":
    tools/mac/build build --target {{target}}

# Package every game for macOS into build/packages/ (debug or release).
[group('build')]
package-mac target="debug":
    tools/mac/build package --target {{target}}

# Write the universal othpatch for macOS into build/packages/ (debug or release).
[group('build')]
package-othpatch-mac target="debug":
    tools/mac/build package-othpatch --target {{target}}

# Run the unit tests on macOS.
[group('test')]
test-mac:
    tools/mac/build test

# Build the Linux games (debug) and run one, such as `just run th06 --data DIR`.
[group('build')]
[no-cd]
[positional-arguments]
run game *args: (_build "linux" "media" "debug")
    #!/usr/bin/env bash
    shift
    exec {{quote(justfile_directory())}}/build/linux/debug/o{{game}} "$@"

[group('test')]
_test image: (_docker_build "linux" image) (_docker_run "linux" image "test")

# Run the unit tests inside the Linux images (all, games or othpatch).
[group('test')]
test programs="all":
    #!/usr/bin/env bash
    set -euo pipefail
    case "{{programs}}" in
        all) ;;
        games) exec just _test media ;;
        othpatch) exec just _test othpatch ;;
        *) echo "Unknown programs {{programs}}: all, games or othpatch" >&2; exit 1 ;;
    esac
    # Runs both images at once. The media image runs the suites that
    # every build has, and the output of othpatch follows that of media.
    just _docker_build linux media
    just _docker_build linux othpatch
    log=$(mktemp)
    trap 'rm -f "$log"' EXIT
    just _docker_run linux othpatch test --no-suite core --no-suite tools > "$log" 2>&1 &
    othpatch=$!
    status=0
    just _docker_run linux media test || status=1
    wait "$othpatch" || status=1
    cat "$log"
    exit "$status"

# Run the tests of the Python tools on this machine.
[group('test')]
test-tools:
    python3 -m unittest discover --start-directory tests/tools --pattern 'test_*.py'

# Run the checks on the whole tree.
[group('test')]
lint:
    prek -a

# Print the version that a build of a program reports.
[group('misc')]
version program="th06":
    @tools/get_version {{program}}

# Delete the build outputs.
[group('misc')]
clean:
    rm -rf build/

