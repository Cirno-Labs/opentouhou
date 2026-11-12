# check=skip=InvalidDefaultArgInFrom
# opentouhou othpatch build image for Linux: the base image with curl and
# mbedTLS, for othpatch and its tests.
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



# mbedTLS supplies TLS for curl. Only othpatch links it.
FROM base AS mbedtls
ARG MBEDTLS_VERSION
ARG MBEDTLS_SHA256
RUN dnf -y install bzip2 && dnf clean all
RUN curl -fsSL -o /tmp/mbedtls.tar.bz2 \
        https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-${MBEDTLS_VERSION}/mbedtls-${MBEDTLS_VERSION}.tar.bz2 \
    && echo "${MBEDTLS_SHA256}  /tmp/mbedtls.tar.bz2" | sha256sum -c - \
    && tar -xjf /tmp/mbedtls.tar.bz2 -C /tmp \
    && cmake -S /tmp/mbedtls-${MBEDTLS_VERSION} -B /tmp/mbedtls-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/ext \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DENABLE_PROGRAMS=OFF \
        -DENABLE_TESTING=OFF \
        -DUSE_SHARED_MBEDTLS_LIBRARY=OFF \
        -DUSE_STATIC_MBEDTLS_LIBRARY=ON \
    && cmake --build /tmp/mbedtls-build -j"$(nproc)" \
    && DESTDIR=/stage cmake --install /tmp/mbedtls-build \
    && rm -rf /tmp/mbedtls.tar.bz2 /tmp/mbedtls-${MBEDTLS_VERSION} /tmp/mbedtls-build



# curl supplies HTTP and HTTPS for othpatch, with mbedTLS for TLS. It has
# no other protocol and no optional library, and no CA bundle of its own:
# othpatch finds the system's certificates at run time.
FROM base AS curl
ARG CURL_VERSION
ARG CURL_SHA256
COPY --from=mbedtls /stage/ext/ /ext/
RUN curl -fsSL -o /tmp/curl.tar.xz \
        https://curl.se/download/curl-${CURL_VERSION}.tar.xz \
    && echo "${CURL_SHA256}  /tmp/curl.tar.xz" | sha256sum -c - \
    && tar -xJf /tmp/curl.tar.xz -C /tmp \
    && cmake -S /tmp/curl-${CURL_VERSION} -B /tmp/curl-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/ext \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_PREFIX_PATH=/ext \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_STATIC_LIBS=ON \
        -DBUILD_CURL_EXE=OFF \
        -DBUILD_TESTING=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_LIBCURL_DOCS=OFF \
        -DBUILD_MISC_DOCS=OFF \
        -DENABLE_CURL_MANUAL=OFF \
        -DCURL_USE_OPENSSL=OFF \
        -DCURL_USE_MBEDTLS=ON \
        -DCURL_CA_BUNDLE=none \
        -DCURL_CA_PATH=none \
        -DCURL_USE_LIBPSL=OFF \
        -DUSE_LIBIDN2=OFF \
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
COPY --from=mbedtls /stage/ext/ /ext/
COPY --from=curl /stage/ext/ /ext/

# Meson adds ccache by itself only when CC is not set.
ENV CC="ccache clang"
LABEL org.opencontainers.image.source="https://github.com/Cirno-Labs/opentouhou"
ENTRYPOINT ["python3.12", "/app/tools/docker/entrypoint.py", "--platform", "linux", "--programs", "othpatch"]
