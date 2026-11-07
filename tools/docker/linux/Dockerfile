# opentouhou build image for Linux.
#
# Multi-stage: each dependency is built in its own stage and installed into
# /ext. The final stage collects /ext and adds the build tools. The source is
# not part of the image; it is mounted at /app.
#
# The base is AlmaLinux 8 (glibc 2.28), so the executables run on every
# distribution with glibc 2.28 or newer.

FROM almalinux:8 AS base
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
FROM base AS sdl
ARG SDL_VERSION=3.4.20
ARG SDL_SHA256=f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f
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



# appimagetool and the AppImage runtime it embeds
FROM base AS appimage
ARG APPIMAGETOOL_VERSION=1.9.1
ARG APPIMAGETOOL_SHA256=ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
ARG RUNTIME_VERSION=20251108
ARG RUNTIME_SHA256=2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d
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
        https://github.com/AppImage/type2-runtime/releases/download/${RUNTIME_VERSION}/runtime-x86_64 \
    && echo "${RUNTIME_SHA256}  /opt/appimage-runtime-x86_64" | sha256sum -c -



# opentouhou
FROM base

RUN dnf -y install \
        ccache \
        file \
        python3.12 \
        python3.12-pip \
    && dnf clean all \
    && python3.12 -m pip install --no-cache-dir \
        meson==1.12.1 \
        ninja==1.13.2

COPY --from=sdl /ext/ /ext/
COPY --from=appimage /opt/ /opt/
RUN ln -s /opt/appimagetool/AppRun /usr/local/bin/appimagetool

RUN mkdir /app
WORKDIR /app

# Meson adds ccache by itself only when CC is not set.
ENV CC="ccache clang"
ENV PKG_CONFIG_PATH=/ext/lib/pkgconfig
ENV APPIMAGE_RUNTIME=/opt/appimage-runtime-x86_64
ENV PYTHONPATH=/app/tools
ENV CCACHE_BASEDIR=/app
ENV CCACHE_COMPILERCHECK=content
ENV CCACHE_DIR=/app/.cache/ccache/linux
LABEL org.opencontainers.image.source="https://github.com/Cirno-Labs/opentouhou"
ENTRYPOINT ["python3.12", "/app/tools/docker/entrypoint.py", "--platform", "linux"]
