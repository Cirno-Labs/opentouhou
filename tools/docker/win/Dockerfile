# opentouhou build image for Windows, cross-compiled with MinGW-w64.
#
# Multi-stage: each dependency is built in its own stage and installed into
# /ext. The final stage collects /ext and adds the build tools. The source is
# not part of the image; it is mounted at /app.
#
# The main target is x86-64. The image also cross-builds a static SDL3
# and zlib for i686, used by the capture tools under replay/. The
# dependencies are static libraries, and the game links libgcc and
# winpthread statically, so the executable needs no DLLs beyond the ones
# Windows has.

FROM debian:trixie AS base
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        gcc-mingw-w64-x86-64-posix \
        g++-mingw-w64-x86-64-posix \
        binutils-mingw-w64-x86-64 \
        gcc-mingw-w64-i686-posix \
        g++-mingw-w64-i686-posix \
        binutils-mingw-w64-i686 \
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



# SDL3_ttf supplies UTF-8 font rasterization.
FROM base AS sdl-ttf
ARG SDL_TTF_VERSION=3.2.2
ARG SDL_TTF_SHA256=63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d
ARG FREETYPE_COMMIT=9973564cfa63763a3e4ac67c09147899539b1e07
ARG FREETYPE_SHA256=026a05a49d114a1235d2926f4c03a9330e4b1a6efe7c217ec9607904c32907d4
ARG HARFBUZZ_COMMIT=564bf9818a18709776856533829c0c04950773d6
ARG HARFBUZZ_SHA256=a448dd6c22d8e1e1cf39438c662251c1f97f810b8780eed4a6d6ada948c99ddc
COPY --from=sdl /ext/ /ext/
RUN curl -fsSL -o /tmp/ttf.tar.gz \
        https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL_TTF_VERSION}/SDL3_ttf-${SDL_TTF_VERSION}.tar.gz \
    && echo "${SDL_TTF_SHA256}  /tmp/ttf.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/ttf.tar.gz -C /tmp \
    && mkdir -p /tmp/SDL3_ttf-${SDL_TTF_VERSION}/external/freetype \
        /tmp/SDL3_ttf-${SDL_TTF_VERSION}/external/harfbuzz \
    && curl -fsSL -o /tmp/freetype.tar.gz \
        https://codeload.github.com/libsdl-org/freetype/tar.gz/${FREETYPE_COMMIT} \
    && echo "${FREETYPE_SHA256}  /tmp/freetype.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/freetype.tar.gz --strip-components=1 \
        -C /tmp/SDL3_ttf-${SDL_TTF_VERSION}/external/freetype \
    && curl -fsSL -o /tmp/harfbuzz.tar.gz \
        https://codeload.github.com/libsdl-org/harfbuzz/tar.gz/${HARFBUZZ_COMMIT} \
    && echo "${HARFBUZZ_SHA256}  /tmp/harfbuzz.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/harfbuzz.tar.gz --strip-components=1 \
        -C /tmp/SDL3_ttf-${SDL_TTF_VERSION}/external/harfbuzz \
    && cmake -S /tmp/SDL3_ttf-${SDL_TTF_VERSION} -B /tmp/ttf-build \
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
        -DCMAKE_PREFIX_PATH=/ext \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DSDLTTF_VENDORED=ON \
        -DSDLTTF_HARFBUZZ=ON \
        -DSDLTTF_PLUTOSVG=OFF \
        -DSDLTTF_SAMPLES=OFF \
    && cmake --build /tmp/ttf-build -j"$(nproc)" \
    && cmake --install /tmp/ttf-build \
    && rm -rf /tmp/ttf.tar.gz /tmp/freetype.tar.gz /tmp/harfbuzz.tar.gz \
        /tmp/SDL3_ttf-${SDL_TTF_VERSION} /tmp/ttf-build



# SDL3 for i686, used by the capture tools.
FROM base AS sdl-i686
ARG SDL_VERSION=3.4.20
ARG SDL_SHA256=f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f
RUN curl -fsSL -o /tmp/sdl.tar.gz \
        https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.tar.gz \
    && echo "${SDL_SHA256}  /tmp/sdl.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/sdl.tar.gz -C /tmp \
    && cmake -S /tmp/SDL3-${SDL_VERSION} -B /tmp/sdl-build \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_SYSTEM_PROCESSOR=x86 \
        -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc-posix \
        -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++-posix \
        -DCMAKE_RC_COMPILER=i686-w64-mingw32-windres \
        -DCMAKE_FIND_ROOT_PATH=/usr/i686-w64-mingw32 \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/ext/i686 \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DSDL_SHARED=OFF \
        -DSDL_STATIC=ON \
        -DSDL_TEST_LIBRARY=OFF \
        -DSDL_TESTS=OFF \
        -DSDL_EXAMPLES=OFF \
    && cmake --build /tmp/sdl-build -j"$(nproc)" \
    && cmake --install /tmp/sdl-build \
    && rm -rf /tmp/sdl.tar.gz /tmp/SDL3-${SDL_VERSION} /tmp/sdl-build



# zlib for i686, used by the capture tools.
FROM base AS zlib-i686
ARG ZLIB_VERSION=1.3.1
ARG ZLIB_SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
RUN curl -fsSL -o /tmp/zlib.tar.gz \
        https://www.zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz \
    && echo "${ZLIB_SHA256}  /tmp/zlib.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/zlib.tar.gz -C /tmp \
    && cd /tmp/zlib-${ZLIB_VERSION} \
    && CC=i686-w64-mingw32-gcc-posix AR=i686-w64-mingw32-ar \
        RANLIB=i686-w64-mingw32-ranlib \
        ./configure --prefix=/ext/i686 --static \
    && make -j"$(nproc)" \
    && make install \
    && rm -rf /tmp/zlib.tar.gz /tmp/zlib-${ZLIB_VERSION}



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



# FFmpeg supplies image codecs and audio demuxers, decoders and conversion.
# Disable assembly so each platform uses the same C implementations.
# Keep the default LGPL configuration; no GPL component is enabled.
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
        --enable-decoder=png,mjpeg,bmp,pcm_u8,pcm_s8,pcm_s16le,pcm_s16be,pcm_u16le,pcm_u16be,pcm_s24le,pcm_s24be,pcm_u24le,pcm_u24be,pcm_s32le,pcm_s32be,pcm_u32le,pcm_u32be,pcm_f32le,pcm_f32be,pcm_f64le,pcm_f64be,pcm_alaw,pcm_mulaw,adpcm_ms,adpcm_ima_wav,vorbis,flac,mp3,mp3float \
        --enable-demuxer=wav,ogg,flac,mp3 \
        --enable-parser=vorbis,flac,mpegaudio \
        --enable-swresample \
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
COPY --from=sdl-ttf /ext/ /ext/
COPY --from=ffmpeg /ext/ /ext/
COPY --from=sdl-i686 /ext/i686/ /ext/i686/
COPY --from=zlib-i686 /ext/i686/ /ext/i686/

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
