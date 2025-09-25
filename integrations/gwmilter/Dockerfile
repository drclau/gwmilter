ARG ALPINE_VERSION=3.22
ARG LIBEGPGCRYPT_COMMIT=49360bf5a07d9aec35f1cf311e9d14f73e01b5b0
ARG LIBEPDFCRYPT_COMMIT=2eacdca2838a67aaada39241d2fda0136a28e2c2

# Stage 1: Build dependencies and gwmilter
FROM alpine:${ALPINE_VERSION} AS builder

# Install build dependencies
RUN apk add --no-cache \
    'build-base~0.5' \
    'glib-dev~2.84' \
    'openssl-dev~3.5' \
    'boost-dev~1.84' \
    'curl-dev~8.14' \
    'fmt-dev~11.2' \
    'gmime-dev~3.2' \
    'gpgme-dev~1.24' \
    'libharu-dev~2.4' \
    'libmilter-dev~1.0' \
    'spdlog-dev~1.15' \
    pkgconf \
    autoconf \
    autoconf-archive \
    automake \
    libtool \
    make \
    cmake \
    git

# Build and install libegpgcrypt
RUN git clone --depth 1 https://github.com/rzvncj/libegpgcrypt.git /deps/libegpgcrypt && \
    cd /deps/libegpgcrypt && \
    git fetch --depth 1 origin ${LIBEGPGCRYPT_COMMIT} && \
    git checkout ${LIBEGPGCRYPT_COMMIT} && \
    ./bootstrap && \
    ./configure && \
    make -j$(nproc) && \
    make install

# Build and install libepdfcrypt
RUN git clone --depth 1 https://github.com/rzvncj/libepdfcrypt.git /deps/libepdfcrypt && \
    cd /deps/libepdfcrypt && \
    git fetch --depth 1 origin ${LIBEPDFCRYPT_COMMIT} && \
    git checkout ${LIBEPDFCRYPT_COMMIT} && \
    ./bootstrap && \
    ./configure && \
    make -j$(nproc) && \
    make install

WORKDIR /app

# Copy the source code
COPY LICENSE README.md CMakeLists.txt /app/
COPY src/ /app/src/

# Build gwmilter
RUN cmake -B build -S . && cmake --build build -- -j$(nproc)

# Stage 2: Create the runtime image
FROM alpine:${ALPINE_VERSION}

# Install runtime dependencies
RUN apk add --no-cache \
    'glib~2.84' \
    'libstdc++~14.2' \
    'boost1.84~1.84' \
    'curl~8.14' \
    'fmt~11.2' \
    'gmime~3.2' \
    'gpgme~1.24' \
    'libharu~2.4' \
    'libmilter~1.0' \
    'spdlog~1.15' \
    'gnupg~2.4' \
    'gettext~0.24'

# Create gwmilter user and set up GnuPG directory
RUN adduser -D -H -s /sbin/nologin gwmilter && \
    mkdir -p /app/gnupg && \
    chown gwmilter:gwmilter /app/gnupg && \
    chmod 700 /app/gnupg

# Copy built artifacts from the builder stage
COPY --from=builder /usr/local/lib/libegpgcrypt.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libepdfcrypt.so* /usr/local/lib/
COPY --from=builder --chown=root:root --chmod=755 /app/build/gwmilter /app/

# Copy supporting files
COPY --chown=root:root --chmod=755 integrations/gwmilter/entrypoint.sh /app/
COPY --chown=gwmilter:gwmilter --chmod=700 data/ /app/data/

# Set the working directory
WORKDIR /app

# Set the environment variables
ENV LD_LIBRARY_PATH=/usr/local/lib
ENV GNUPGHOME=/app/gnupg

# Expose the Milter port
EXPOSE 10025

ENTRYPOINT ["/app/entrypoint.sh"]
