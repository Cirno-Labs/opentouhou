# check=skip=InvalidDefaultArgInFrom
# opentouhou media build image for Windows: the base image with SDL3_ttf
# and FFmpeg, for the games, the replay runner and their tests. It also
# has MinGW-w64 for i686 with a static SDL3 and zlib, for the capture
# tools under replay/.
#
# Multi-stage: each dependency is built in its own stage on the base image
# and installed into /stage/ext, so that the final stage adds only the new
# files to the base's /ext. The ARGs come from tools/docker/pins, and
# BASE_IMAGE names the base image of the same tag that tools/docker/image
# computes.

# BASE_IMAGE has no default, so the build fails without it instead of
# picking a base image of another tag.
ARG BASE_IMAGE
FROM ${BASE_IMAGE} AS base



# SDL3_ttf supplies UTF-8 font rasterization.
FROM base AS sdl-ttf
ARG SDL_TTF_VERSION
ARG SDL_TTF_SHA256
ARG FREETYPE_COMMIT
ARG FREETYPE_SHA256
ARG HARFBUZZ_COMMIT
ARG HARFBUZZ_SHA256
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
    && DESTDIR=/stage cmake --install /tmp/ttf-build \
    && rm -rf /tmp/ttf.tar.gz /tmp/freetype.tar.gz /tmp/harfbuzz.tar.gz \
        /tmp/SDL3_ttf-${SDL_TTF_VERSION} /tmp/ttf-build



# FFmpeg supplies image codecs and audio demuxers, decoders and conversion.
# Disable assembly so each platform uses the same C implementations.
# Keep the default LGPL configuration; no GPL component is enabled.
FROM base AS ffmpeg
ARG FFMPEG_VERSION
ARG FFMPEG_SHA256
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
    && ./configure \
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
    && make install DESTDIR=/stage \
    && rm -rf /tmp/ffmpeg.tar.xz /tmp/ffmpeg-${FFMPEG_VERSION}



# MinGW-w64 for i686, for the capture tools.
FROM base AS toolchain-i686
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        gcc-mingw-w64-i686-posix \
        g++-mingw-w64-i686-posix \
        binutils-mingw-w64-i686 \
    && rm -rf /var/lib/apt/lists/*



# SDL3 for i686, used by the capture tools.
FROM toolchain-i686 AS sdl-i686
ARG SDL_VERSION
ARG SDL_SHA256
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
    && DESTDIR=/stage cmake --install /tmp/sdl-build \
    && rm -rf /tmp/sdl.tar.gz /tmp/SDL3-${SDL_VERSION} /tmp/sdl-build



# zlib for i686, used by the capture tools.
FROM toolchain-i686 AS zlib-i686
ARG ZLIB_VERSION
ARG ZLIB_SHA256
RUN curl -fsSL -o /tmp/zlib.tar.gz \
        https://www.zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz \
    && echo "${ZLIB_SHA256}  /tmp/zlib.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/zlib.tar.gz -C /tmp \
    && cd /tmp/zlib-${ZLIB_VERSION} \
    && CC=i686-w64-mingw32-gcc-posix AR=i686-w64-mingw32-ar \
        RANLIB=i686-w64-mingw32-ranlib \
        ./configure --prefix=/ext/i686 --static \
    && make -j"$(nproc)" \
    && make install DESTDIR=/stage \
    && rm -rf /tmp/zlib.tar.gz /tmp/zlib-${ZLIB_VERSION}



# opentouhou games
FROM toolchain-i686
COPY --from=sdl-ttf /stage/ext/ /ext/
COPY --from=ffmpeg /stage/ext/ /ext/
COPY --from=sdl-i686 /stage/ext/i686/ /ext/i686/
COPY --from=zlib-i686 /stage/ext/i686/ /ext/i686/
LABEL org.opencontainers.image.source="https://github.com/Cirno-Labs/opentouhou"
ENTRYPOINT ["python3", "/app/tools/docker/entrypoint.py", "--platform", "win", "--programs", "games"]
