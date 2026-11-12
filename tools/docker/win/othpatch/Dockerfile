# check=skip=InvalidDefaultArgInFrom
# opentouhou othpatch build image for Windows: the base image with curl,
# for othpatch and its tests.
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



# curl supplies HTTP and HTTPS for othpatch, with Schannel for TLS and the
# Windows certificate store. It has no other protocol and no optional
# library. Only othpatch links it.
FROM base AS curl
ARG CURL_VERSION
ARG CURL_SHA256
RUN curl -fsSL -o /tmp/curl.tar.xz \
        https://curl.se/download/curl-${CURL_VERSION}.tar.xz \
    && echo "${CURL_SHA256}  /tmp/curl.tar.xz" | sha256sum -c - \
    && tar -xJf /tmp/curl.tar.xz -C /tmp \
    && cmake -S /tmp/curl-${CURL_VERSION} -B /tmp/curl-build \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc-posix \
        -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
        -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/ext \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_STATIC_LIBS=ON \
        -DBUILD_CURL_EXE=OFF \
        -DBUILD_TESTING=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_LIBCURL_DOCS=OFF \
        -DBUILD_MISC_DOCS=OFF \
        -DENABLE_CURL_MANUAL=OFF \
        -DCURL_USE_PKGCONFIG=OFF \
        -DCURL_USE_OPENSSL=OFF \
        -DCURL_USE_SCHANNEL=ON \
        -DCURL_CA_BUNDLE=none \
        -DCURL_CA_PATH=none \
        -DCURL_USE_LIBPSL=OFF \
        -DUSE_LIBIDN2=OFF \
        -DUSE_WIN32_IDN=OFF \
        -DCURL_USE_LIBSSH2=OFF \
        -DCURL_USE_LIBSSH=OFF \
        -DUSE_NGHTTP2=OFF \
        -DCURL_ZLIB=OFF \
        -DCURL_BROTLI=OFF \
        -DCURL_ZSTD=OFF \
        -DCURL_DISABLE_COOKIES=ON \
        -DENABLE_UNIX_SOCKETS=OFF \
        -DCURL_ENABLE_NTLM=OFF \
        -DCURL_DISABLE_DICT=ON \
        -DCURL_DISABLE_FILE=ON \
        -DCURL_DISABLE_FTP=ON \
        -DCURL_DISABLE_GOPHER=ON \
        -DCURL_DISABLE_IMAP=ON \
        -DCURL_DISABLE_IPFS=ON \
        -DCURL_DISABLE_LDAP=ON \
        -DCURL_DISABLE_LDAPS=ON \
        -DCURL_DISABLE_MQTT=ON \
        -DCURL_DISABLE_POP3=ON \
        -DCURL_DISABLE_RTSP=ON \
        -DCURL_DISABLE_SMTP=ON \
        -DCURL_DISABLE_TELNET=ON \
        -DCURL_DISABLE_TFTP=ON \
        -DCURL_DISABLE_WEBSOCKETS=ON \
    && cmake --build /tmp/curl-build -j"$(nproc)" \
    && DESTDIR=/stage cmake --install /tmp/curl-build \
    && rm -rf /tmp/curl.tar.xz /tmp/curl-${CURL_VERSION} /tmp/curl-build



# opentouhou othpatch
FROM base
COPY --from=curl /stage/ext/ /ext/
LABEL org.opencontainers.image.source="https://github.com/Cirno-Labs/opentouhou"
ENTRYPOINT ["python3", "/app/tools/docker/entrypoint.py", "--platform", "win", "--programs", "othpatch"]
