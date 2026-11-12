# check=skip=InvalidDefaultArgInFrom
# opentouhou media build image for Linux: the base image with SDL3_ttf,
# FFmpeg and appimagetool, for the games, the replay runner and their
# tests.
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
RUN curl -fsSL -o /tmp/ffmpeg.tar.xz \
        https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz \
    && echo "${FFMPEG_SHA256}  /tmp/ffmpeg.tar.xz" | sha256sum -c - \
    && tar -xJf /tmp/ffmpeg.tar.xz -C /tmp \
    && cd /tmp/ffmpeg-${FFMPEG_VERSION} \
    && ./configure \
        --prefix=/ext \
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
        --pkg-config-flags="--static" \
    && make -j"$(nproc)" \
    && make install DESTDIR=/stage \
    && rm -rf /tmp/ffmpeg.tar.xz /tmp/ffmpeg-${FFMPEG_VERSION}



# appimagetool and the AppImage runtime it embeds
FROM base AS appimage
ARG APPIMAGETOOL_VERSION
ARG APPIMAGETOOL_SHA256
ARG APPIMAGE_RUNTIME_VERSION
ARG APPIMAGE_RUNTIME_SHA256
# Docker has no FUSE, so the tool is unpacked instead of mounted.
RUN curl -fsSL -o /tmp/appimagetool \
        https://github.com/AppImage/appimagetool/releases/download/${APPIMAGETOOL_VERSION}/appimagetool-x86_64.AppImage \
    && echo "${APPIMAGETOOL_SHA256}  /tmp/appimagetool" | sha256sum -c - \
    && chmod +x /tmp/appimagetool \
    && cd /opt \
    && /tmp/appimagetool --appimage-extract \
    && mv /opt/squashfs-root /opt/appimagetool \
    && rm /tmp/appimagetool
RUN curl -fsSL -o /opt/appimage-runtime-x86_64 \
        https://github.com/AppImage/type2-runtime/releases/download/${APPIMAGE_RUNTIME_VERSION}/runtime-x86_64 \
    && echo "${APPIMAGE_RUNTIME_SHA256}  /opt/appimage-runtime-x86_64" | sha256sum -c -



# opentouhou games
FROM base
RUN dnf -y install file && dnf clean all
COPY --from=sdl-ttf /stage/ext/ /ext/
COPY --from=ffmpeg /stage/ext/ /ext/
COPY --from=appimage /opt/ /opt/
RUN ln -s /opt/appimagetool/AppRun /usr/local/bin/appimagetool

# Meson adds ccache by itself only when CC is not set.
ENV CC="ccache clang"
ENV APPIMAGE_RUNTIME=/opt/appimage-runtime-x86_64
LABEL org.opencontainers.image.source="https://github.com/Cirno-Labs/opentouhou"
ENTRYPOINT ["python3.12", "/app/tools/docker/entrypoint.py", "--platform", "linux", "--programs", "games"]
