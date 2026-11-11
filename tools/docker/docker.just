# Use a local image, else pull it, else build it. force=1 always builds.
[group('docker')]
_docker_build platform force="0":
    #!/usr/bin/env sh
    set -e
    full_tag="$(tools/docker/image_tag {{platform}})"
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
    source="https://github.com/${GITHUB_REPOSITORY:-Cirno-Labs/opentouhou}"
    docker build \
        -f tools/docker/{{platform}}/Dockerfile \
        --label "org.opencontainers.image.source=$source" \
        -t "$full_tag" \
        tools/docker/{{platform}}

[group('docker')]
_docker_push platform:
    docker push "$(tools/docker/image_tag {{platform}})"

[group('docker')]
_docker_run platform *args:
    #!/usr/bin/env sh
    set -e
    docker run \
        --rm \
        --user {{HOST_USER_UID}}:{{HOST_USER_GID}} \
        -e CCACHE_MAXSIZE \
        -v "{{CWD}}:/app" \
        "$(tools/docker/image_tag {{platform}})" \
        {{args}}

# Build the Linux image locally.
[group('docker')]
image-linux force="1": (_docker_build "linux" force)

# Build the Windows image locally.
[group('docker')]
image-win force="1": (_docker_build "win" force)

# Push the Linux image to the registry.
[group('docker')]
push-image-linux: (image-linux "0") (_docker_push "linux")

# Push the Windows image to the registry.
[group('docker')]
push-image-win: (image-win "0") (_docker_push "win")
