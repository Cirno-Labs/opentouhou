# opentouhou base build image for Linux: the compiler, Meson, Ninja and
# ccache, with SDL3 and zlib, which every program links. The media and
# othpatch images are built on it.
#
# Multi-stage: each dependency is built in its own stage and installed into
# /ext. The final stage collects /ext and adds the build tools. The source is
# not part of the image; it is mounted at /app. The ARGs come from
# tools/docker/pins.
#
# The base is AlmaLinux 8 (glibc 2.28), so the executables run on every
# distribution with glibc 2.28 or newer.

FROM almalinux:8 AS toolchain
RUN dnf -y install epel-release dnf-plugins-core \
    && dnf config-manager --set-enabled powertools \
    && dnf -y install \
        clang \
        lld \
        cmake \
        make \
        git \
        pkgconf-pkg-config \
    && dnf clean all
ENV CC=clang
ENV CXX=clang++



# SDL3
FROM toolchain AS sdl
ARG SDL_VERSION
ARG SDL_SHA256
# SDL3 loads the X11, Wayland, audio and GL libraries at run time. The
# headers are needed only to build the backends.
RUN dnf -y install \
        alsa-lib-devel \
        dbus-devel \
        libX11-devel \
        libXcursor-devel \
        libXext-devel \
        libXfixes-devel \
        libXi-devel \
        libXrandr-devel \
        libXScrnSaver-devel \
        libXtst-devel \
        libdrm-devel \
        libxkbcommon-devel \
        mesa-libEGL-devel \
        mesa-libGL-devel \
        mesa-libgbm-devel \
        pulseaudio-libs-devel \
        systemd-devel \
        wayland-devel \
        wayland-protocols-devel \
    && dnf clean all
RUN curl -fsSL -o /tmp/sdl.tar.gz \
        https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.tar.gz \
    && echo "${SDL_SHA256}  /tmp/sdl.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/sdl.tar.gz -C /tmp \
    && cmake -S /tmp/SDL3-${SDL_VERSION} -B /tmp/sdl-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/ext \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DSDL_SHARED=OFF \
        -DSDL_STATIC=ON \
        -DSDL_TEST_LIBRARY=OFF \
        -DSDL_TESTS=OFF \
        -DSDL_EXAMPLES=OFF \
    && cmake --build /tmp/sdl-build -j"$(nproc)" \
    && cmake --install /tmp/sdl-build \
    && rm -rf /tmp/sdl.tar.gz /tmp/SDL3-${SDL_VERSION} /tmp/sdl-build



# zlib, built statically for othpatch, the case tools and FFmpeg's PNG
# encoder and decoder.
FROM toolchain AS zlib
ARG ZLIB_VERSION
ARG ZLIB_SHA256
RUN curl -fsSL -o /tmp/zlib.tar.gz \
        https://www.zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz \
    && echo "${ZLIB_SHA256}  /tmp/zlib.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/zlib.tar.gz -C /tmp \
    && cd /tmp/zlib-${ZLIB_VERSION} \
    && CFLAGS="-fPIC" ./configure --prefix=/ext --static \
    && make -j"$(nproc)" \
    && make install \
    && rm -rf /tmp/zlib.tar.gz /tmp/zlib-${ZLIB_VERSION}



# opentouhou base
FROM toolchain
ARG MESON_VERSION
ARG NINJA_VERSION

RUN dnf -y install \
        ccache \
        python3.12 \
        python3.12-pip \
    && dnf clean all \
    && python3.12 -m pip install --no-cache-dir \
        meson==${MESON_VERSION} \
        ninja==${NINJA_VERSION}

COPY --from=sdl /ext/ /ext/
COPY --from=zlib /ext/ /ext/

RUN mkdir /app
WORKDIR /app

ENV PKG_CONFIG_PATH=/ext/lib/pkgconfig
ENV PYTHONPATH=/app/tools
ENV CCACHE_BASEDIR=/app
ENV CCACHE_COMPILERCHECK=content
ENV CCACHE_DIR=/app/.cache/ccache/linux
LABEL org.opencontainers.image.source="https://github.com/Cirno-Labs/opentouhou"
