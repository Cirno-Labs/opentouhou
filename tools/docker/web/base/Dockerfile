# opentouhou base build image for the web: Emscripten, Meson, Ninja and
# ccache, with SDL3 and zlib. The media image is built on it; othpatch
# has no web build.
#
# Multi-stage: each dependency is built in its own stage and installed into
# /ext. The final stage collects /ext and adds the build tools. The source is
# not part of the image; it is mounted at /app. The ARGs come from
# tools/docker/pins.
#
# Every library is built with -pthread, because the audio engine runs on
# an SDL thread, and wasm-ld links shared memory only from objects that
# all have atomics.

ARG EMSDK_VERSION
ARG EMSDK_SHA256
FROM emscripten/emsdk:${EMSDK_VERSION}@sha256:${EMSDK_SHA256} AS toolchain
RUN apt-get update \
    && apt-get install -y --no-install-recommends ccache pkgconf \
    && rm -rf /var/lib/apt/lists/*
ENV CFLAGS=-pthread
ENV CXXFLAGS=-pthread
ENV LDFLAGS=-pthread



# SDL3
FROM toolchain AS sdl
ARG SDL_VERSION
ARG SDL_SHA256
RUN curl -fsSL -o /tmp/sdl.tar.gz \
        https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.tar.gz \
    && echo "${SDL_SHA256}  /tmp/sdl.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/sdl.tar.gz -C /tmp \
    && emcmake cmake -S /tmp/SDL3-${SDL_VERSION} -B /tmp/sdl-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/ext \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DSDL_SHARED=OFF \
        -DSDL_STATIC=ON \
        -DSDL_THREADS=ON \
        -DSDL_PTHREADS=ON \
        -DSDL_TEST_LIBRARY=OFF \
        -DSDL_TESTS=OFF \
        -DSDL_EXAMPLES=OFF \
    && cmake --build /tmp/sdl-build -j"$(nproc)" \
    && cmake --install /tmp/sdl-build \
    && rm -rf /tmp/sdl.tar.gz /tmp/SDL3-${SDL_VERSION} /tmp/sdl-build



# zlib, for FFmpeg's PNG encoder and decoder and the game zips.
FROM toolchain AS zlib
ARG ZLIB_VERSION
ARG ZLIB_SHA256
RUN curl -fsSL -o /tmp/zlib.tar.gz \
        https://www.zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz \
    && echo "${ZLIB_SHA256}  /tmp/zlib.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/zlib.tar.gz -C /tmp \
    && cd /tmp/zlib-${ZLIB_VERSION} \
    && emconfigure ./configure --prefix=/ext --static \
    && emmake make -j"$(nproc)" libz.a \
    && emmake make install \
    && rm -rf /tmp/zlib.tar.gz /tmp/zlib-${ZLIB_VERSION}



# opentouhou base
FROM toolchain
ARG MESON_VERSION
ARG NINJA_VERSION

RUN python3 -m pip install --no-cache-dir --break-system-packages \
        meson==${MESON_VERSION} \
        ninja==${NINJA_VERSION}

COPY --from=sdl /ext/ /ext/
COPY --from=zlib /ext/ /ext/

# The build runs as the host's user, which writes the libraries that
# Emscripten builds on first use into its cache.
RUN chmod -R a+rwX /emsdk/upstream/emscripten/cache

RUN mkdir -p /app
WORKDIR /app

ENV PKG_CONFIG_PATH=/ext/lib/pkgconfig
ENV PYTHONPATH=/app/tools
ENV CCACHE_BASEDIR=/app
ENV CCACHE_COMPILERCHECK=content
ENV CCACHE_DIR=/app/.cache/ccache/web
LABEL org.opencontainers.image.source="https://github.com/Cirno-Labs/opentouhou"
