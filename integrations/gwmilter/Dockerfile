ARG ALPINE_VERSION=3.21
ARG LIBEGPGCRYPT_COMMIT=<insert-libegpgcrypt-commit-hash>
ARG LIBEPDFCRYPT_COMMIT=<insert-libepdfcrypt-commit-hash>

# Stage 1: Build dependencies and gwmilter
FROM alpine:${ALPINE_VERSION} AS builder

# Install build dependencies
RUN apk add --no-cache \
    build-base \
    glib-dev \
    openssl-dev \
    boost-dev \
    curl-dev \
    fmt-dev \
    gmime-dev \
    gpgme-dev \
    libharu-dev \
    libmilter-dev \
    spdlog-dev \
    pkgconf \
    autoconf \
    autoconf-archive \
    automake \
    libtool \
    make \
    cmake \
    git

# Build and install libegpgcrypt
RUN git clone --depth 1 https://github.com/drclau/libegpgcrypt.git /deps/libegpgcrypt && \
    cd /deps/libegpgcrypt && \
    git checkout ${LIBEGPGCRYPT_COMMIT} && \
    ./bootstrap && \
    ./configure && \
    make -j$(nproc) && \
    make install

# Build and install libepdfcrypt
RUN git clone --depth 1 https://github.com/drclau/libepdfcrypt.git /deps/libepdfcrypt && \
    cd /deps/libepdfcrypt && \
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
    glib \
    libstdc++ \
    boost \
    curl \
    fmt \
    gmime \
    gpgme \
    libharu \
    libmilter \
    spdlog \
    gnupg \
    gettext

# Create gwmilter user and set up GnuPG directory
RUN adduser -D -H -s /sbin/nologin gwmilter && \
    mkdir -p /app/gnupg && \
    chown gwmilter:gwmilter /app/gnupg && \
    chmod 700 /app/gnupg

# Copy built artifacts from the builder stage
COPY --from=builder /usr/local/lib /usr/local/lib
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
