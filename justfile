CWD := `pwd`
HOST_USER_UID := `id -u`
HOST_USER_GID := `id -g`
DOCKER_IMAGE_VERSION := "20261107"
DOCKER_IMAGE_PREFIX := "ghcr.io/cirno-labs/opentouhou-"

# List the recipes.
default:
    @just --list

# Use a local image, else pull it, else build it. force=1 always builds.
_docker_build platform force="0":
    #!/usr/bin/env sh
    set -e
    full_tag="{{DOCKER_IMAGE_PREFIX}}{{platform}}:{{DOCKER_IMAGE_VERSION}}"
    if [ "{{force}}" = "0" ]; then
        if docker image inspect "$full_tag" >/dev/null 2>&1; then
            exit 0
        fi
        echo "Docker image $full_tag not found, trying to pull it"
        if docker pull "$full_tag"; then
            exit 0
        fi
        echo "Cannot pull $full_tag, building it"
    fi
    docker build \
        -f tools/docker/{{platform}}/Dockerfile \
        -t "$full_tag" \
        tools/docker/{{platform}}

_docker_push platform:
    docker push {{DOCKER_IMAGE_PREFIX}}{{platform}}:{{DOCKER_IMAGE_VERSION}}

_docker_run platform *args:
    docker run \
        --rm \
        --user {{HOST_USER_UID}}:{{HOST_USER_GID}} \
        -e CCACHE_MAXSIZE \
        -v "{{CWD}}:/app" \
        {{DOCKER_IMAGE_PREFIX}}{{platform}}:{{DOCKER_IMAGE_VERSION}} \
        {{args}}

# Build the Linux image locally.
image-linux force="1": (_docker_build "linux" force)

# Build the Windows image locally.
image-win force="1": (_docker_build "win" force)

# Push the Linux image to the registry.
push-image-linux: (image-linux "0") (_docker_push "linux")

# Push the Windows image to the registry.
push-image-win: (image-win "0") (_docker_push "win")

# Build every game for Linux (debug or release).
build-linux target="debug": (image-linux "0") (_docker_run "linux" "build" "--target" target)

# Build every game for Windows (debug or release).
build-win target="debug": (image-win "0") (_docker_run "win" "build" "--target" target)

# Package every game for Linux into build/packages/.
package-linux target="debug": (build-linux target) (_docker_run "linux" "package" "--target" target)

# Package every game for Windows into build/packages/.
package-win target="debug": (build-win target) (_docker_run "win" "package" "--target" target)

# Run the unit tests inside the Linux image.
test: (image-linux "0") (_docker_run "linux" "test")

# Run the checks on the whole tree.
lint:
    prek -a

# Print the version that the packages carry.
version:
    @tools/get_version

# Delete the build outputs.
clean:
    rm -rf build/
