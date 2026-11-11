CWD := `pwd`
HOST_USER_UID := `id -u`
HOST_USER_GID := `id -g`

import "tools/docker/docker.just"

mod replay

# List the recipes.
[group('misc')]
default:
    @just --list

# Build every game for Linux (debug or release).
[group('build')]
build-linux target="debug": (image-linux "0") (_docker_run "linux" "build" "--target" target)

# Build every game for Windows (debug or release).
[group('build')]
build-win target="debug": (image-win "0") (_docker_run "win" "build" "--target" target)

# Package every game for Linux into build/packages/.
[group('build')]
package-linux target="debug": (build-linux target) (_docker_run "linux" "package" "--target" target)

# Package every game for Windows into build/packages/.
[group('build')]
package-win target="debug": (build-win target) (_docker_run "win" "package" "--target" target)

# Build the macOS dependencies into build/mac-deps (on a Mac).
[group('build')]
deps-mac:
    tools/mac/build deps

# Build every game for macOS, arm64 and x86-64 (debug or release).
[group('build')]
build-mac target="debug":
    tools/mac/build build --target {{target}}

# Package every game for macOS into build/packages/.
[group('build')]
package-mac target="debug":
    tools/mac/build package --target {{target}}

# Run the unit tests on macOS.
[group('test')]
test-mac:
    tools/mac/build test

# Run the unit tests inside the Linux image.
[group('test')]
test: (image-linux "0") (_docker_run "linux" "test")

# Run the checks on the whole tree.
[group('test')]
lint:
    prek -a

# Print the version that the packages carry.
[group('misc')]
version:
    @tools/get_version

# Delete the build outputs.
[group('misc')]
clean:
    rm -rf build/

