# opentouhou base build image for Windows, cross-compiled with MinGW-w64
# for x86-64: the compiler, Meson, Ninja and ccache, with SDL3 and zlib,
# which every program links. The media and othpatch images are built on
# it.
#
# Multi-stage: each dependency is built in its own stage and installed into
# /ext. The final stage collects /ext and adds the build tools. The source is
# not part of the image; it is mounted at /app. The ARGs come from
# tools/docker/pins.
#
# The dependencies are static libraries, and the programs link libgcc and
# winpthread statically, so the executables need no DLLs beyond the ones
# Windows has.

FROM debian:trixie AS toolchain
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        gcc-mingw-w64-x86-64-posix \
        g++-mingw-w64-x86-64-posix \
        binutils-mingw-w64-x86-64 \
        make \
        cmake \
        pkg-config \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*



# SDL3
FROM toolchain AS sdl
ARG SDL_VERSION
ARG SDL_SHA256
RUN curl -fsSL -o /tmp/sdl.tar.gz \
        https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.tar.gz \
    && echo "${SDL_SHA256}  /tmp/sdl.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/sdl.tar.gz -C /tmp \
    && cmake -S /tmp/SDL3-${SDL_VERSION} -B /tmp/sdl-build \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc-posix \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++-posix \
        -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
        -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/ext \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DSDL_SHARED=OFF \
        -DSDL_STATIC=ON \
        -DSDL_TEST_LIBRARY=OFF \
        -DSDL_TESTS=OFF \
        -DSDL_EXAMPLES=OFF \
    && cmake --build /tmp/sdl-build -j"$(nproc)" \
    && cmake --install /tmp/sdl-build \
    && rm -rf /tmp/sdl.tar.gz /tmp/SDL3-${SDL_VERSION} /tmp/sdl-build



# zlib, cross-compiled statically for othpatch and FFmpeg's PNG encoder
# and decoder.
FROM toolchain AS zlib
ARG ZLIB_VERSION
ARG ZLIB_SHA256
RUN curl -fsSL -o /tmp/zlib.tar.gz \
        https://www.zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz \
    && echo "${ZLIB_SHA256}  /tmp/zlib.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/zlib.tar.gz -C /tmp \
    && cd /tmp/zlib-${ZLIB_VERSION} \
    && CC=x86_64-w64-mingw32-gcc-posix AR=x86_64-w64-mingw32-ar \
        RANLIB=x86_64-w64-mingw32-ranlib \
        ./configure --prefix=/ext --static \
    && make -j"$(nproc)" \
    && make install \
    && rm -rf /tmp/zlib.tar.gz /tmp/zlib-${ZLIB_VERSION}



# opentouhou base
FROM toolchain
ARG MESON_VERSION
ARG NINJA_VERSION

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ccache \
        git \
        python3 \
        python3-pip \
    && rm -rf /var/lib/apt/lists/* \
    && python3 -m pip install --no-cache-dir --break-system-packages \
        meson==${MESON_VERSION} \
        ninja==${NINJA_VERSION}

COPY --from=sdl /ext/ /ext/
COPY --from=zlib /ext/ /ext/

RUN mkdir /app
WORKDIR /app

ENV PKG_CONFIG_LIBDIR=/ext/lib/pkgconfig
ENV PKG_CONFIG_PATH=/ext/lib/pkgconfig
ENV PYTHONPATH=/app/tools
ENV CCACHE_BASEDIR=/app
ENV CCACHE_COMPILERCHECK=content
ENV CCACHE_DIR=/app/.cache/ccache/win
LABEL org.opencontainers.image.source="https://github.com/Cirno-Labs/opentouhou"
