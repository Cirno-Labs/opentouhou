# The patched DOSBox-X of the dosbox backend: the pinned tag with
# dosbox-x.patch, host.cpp, host.h and hook.h, as a native SDL2 build in
# /opt/dosbox-x. The final stage keeps its run-time libraries and a C
# compiler and nasm for the backend libraries. Runs headless with SDL's
# dummy drivers. The build context is replay/dosbox.

FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
        autoconf \
        automake \
        build-essential \
        ca-certificates \
        git \
        libfreetype-dev \
        libncurses-dev \
        libpng-dev \
        libsdl2-dev \
        libtool \
        libx11-dev \
        libxkbfile-dev \
        libxrandr-dev \
        pkg-config \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*
ARG DOSBOX_X_TAG=dosbox-x-v2026.10.01
ARG DOSBOX_X_COMMIT=b6abbd5980a885f5f310a4088c59a8688d1b116c
RUN git clone --depth 1 --branch "$DOSBOX_X_TAG" \
        https://github.com/joncampbell123/dosbox-x.git /src \
    && test "$(git -C /src rev-parse HEAD)" = "$DOSBOX_X_COMMIT"
WORKDIR /src
RUN ./autogen.sh
COPY dosbox-x.patch hook.h host.h host.cpp /harness/
RUN git apply /harness/dosbox-x.patch \
    && cp /harness/hook.h /harness/host.h include/ \
    && cp /harness/host.cpp src/misc/ \
    && autoreconf -fi \
    && ./configure --prefix=/opt/dosbox-x --enable-sdl2 --disable-debug \
        --disable-sdlnet --disable-libslirp --disable-libfluidsynth \
        --disable-avcodec --disable-mt32 --disable-printer --disable-opengl \
    && make -j"$(nproc)" \
    && make install

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        libfreetype6 \
        libncursesw6 \
        libpng16-16t64 \
        libsdl2-2.0-0 \
        libx11-6 \
        libxkbfile1 \
        libxrandr2 \
        nasm \
        zlib1g \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /opt/dosbox-x /opt/dosbox-x
RUN ! ldd /opt/dosbox-x/bin/dosbox-x | grep 'not found'
ENV PATH=/opt/dosbox-x/bin:$PATH \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy
