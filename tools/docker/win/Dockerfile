# opentouhou build image for Windows, cross-compiled with MinGW-w64.
#
# Multi-stage: each dependency is built in its own stage and installed into
# /ext. The final stage collects /ext and adds the build tools. The source is
# not part of the image; it is mounted at /app.
#
# The target is x86-64. The dependencies are static libraries, and the game
# links libgcc and winpthread statically, so the executable needs no DLLs
# beyond the ones Windows has.

FROM debian:trixie AS base
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
FROM base AS sdl
ARG SDL_VERSION=3.4.20
ARG SDL_SHA256=f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f
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



# zlib, cross-compiled statically for FFmpeg's PNG encoder and decoder.
FROM base AS zlib
ARG ZLIB_VERSION=1.3.1
ARG ZLIB_SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
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



# FFmpeg, cross-compiled statically with only the codecs core/image needs:
# PNG, MJPEG and BMP decoding, PNG and MJPEG encoding, through libavcodec,
# libavutil and libswscale. No assembly, so every platform decodes with the
# same C code. No libavformat: core/image demuxes nothing, it decodes a
# whole buffer as one frame. The default configuration is LGPL,
# since --enable-gpl is never passed.
FROM base AS ffmpeg
ARG FFMPEG_VERSION=7.1.1
ARG FFMPEG_SHA256=733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1
COPY --from=zlib /ext/ /ext/
# configure builds helper tools for the build machine, so it needs a native
# compiler besides the cross one.
RUN apt-get update \
    && apt-get install -y --no-install-recommends gcc libc6-dev \
    && rm -rf /var/lib/apt/lists/*
RUN curl -fsSL -o /tmp/ffmpeg.tar.xz \
        https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz \
    && echo "${FFMPEG_SHA256}  /tmp/ffmpeg.tar.xz" | sha256sum -c - \
    && tar -xJf /tmp/ffmpeg.tar.xz -C /tmp \
    && cd /tmp/ffmpeg-${FFMPEG_VERSION} \
    && PKG_CONFIG_PATH=/ext/lib/pkgconfig ./configure \
        --prefix=/ext \
        --arch=x86_64 \
        --target-os=mingw32 \
        --cross-prefix=x86_64-w64-mingw32- \
        --cc=x86_64-w64-mingw32-gcc-posix \
        --enable-cross-compile \
        --disable-everything \
        --disable-programs \
        --disable-doc \
        --disable-network \
        --disable-autodetect \
        --disable-x86asm \
        --disable-shared \
        --enable-static \
        --enable-pic \
        --enable-zlib \
        --enable-decoder=png,mjpeg,bmp \
        --enable-encoder=png,mjpeg \
        --pkg-config=pkg-config \
        --pkg-config-flags="--static" \
    && make -j"$(nproc)" \
    && make install \
    && rm -rf /tmp/ffmpeg.tar.xz /tmp/ffmpeg-${FFMPEG_VERSION}



# opentouhou
FROM base

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ccache \
        file \
        git \
        python3 \
        python3-pip \
    && rm -rf /var/lib/apt/lists/* \
    && python3 -m pip install --no-cache-dir --break-system-packages \
        meson==1.12.1 \
        ninja==1.13.2

COPY --from=sdl /ext/ /ext/
COPY --from=ffmpeg /ext/ /ext/

RUN mkdir /app
WORKDIR /app

ENV PKG_CONFIG_LIBDIR=/ext/lib/pkgconfig
ENV PKG_CONFIG_PATH=/ext/lib/pkgconfig
ENV PYTHONPATH=/app/tools
ENV CCACHE_BASEDIR=/app
ENV CCACHE_COMPILERCHECK=content
ENV CCACHE_DIR=/app/.cache/ccache/win
LABEL org.opencontainers.image.source="https://github.com/Cirno-Labs/opentouhou"
ENTRYPOINT ["python3", "/app/tools/docker/entrypoint.py", "--platform", "win"]
