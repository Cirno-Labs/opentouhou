# Use a local image, else pull it, else build it. force=1 always builds.
[group('docker')]
_docker_build platform image force="0":
    tools/docker/image build {{platform}} {{image}} {{ if force == "1" { "--force" } else { "" } }}

[group('docker')]
_docker_run platform image *args:
    #!/usr/bin/env sh
    set -e
    docker run \
        --rm \
        --user {{HOST_USER_UID}}:{{HOST_USER_GID}} \
        -e CCACHE_MAXSIZE \
        -e RELEASES_URL \
        -e RELEASE_INDEX_URL \
        -e OPENTOUHOU_RELEASE \
        -v "{{CWD}}:/app" \
        "$(tools/docker/image tag {{platform}} {{image}})" \
        {{args}}

# Build the Linux images locally: base, media and othpatch.
[group('docker')]
image-linux force="1": (_docker_build "linux" "media" force) (_docker_build "linux" "othpatch" force)

# Build the Windows images locally: base, media and othpatch.
[group('docker')]
image-win force="1": (_docker_build "win" "media" force) (_docker_build "win" "othpatch" force)

# Push the Linux images to the registry.
[group('docker')]
push-image-linux: (image-linux "0")
    for image in base media othpatch; do tools/docker/image push linux $image; done

# Push the Windows images to the registry.
[group('docker')]
push-image-win: (image-win "0")
    for image in base media othpatch; do tools/docker/image push win $image; done
