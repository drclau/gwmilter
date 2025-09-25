ARG ALPINE_VERSION=3.22

FROM alpine:${ALPINE_VERSION}

# Install runtime dependencies for key generation
RUN apk add --no-cache \
    'gnupg~2.4'

# Create gwmilter user for consistent user management across services
RUN adduser -D -H -s /sbin/nologin gwmilter && \
    mkdir -p /app/gnupg && \
    chown gwmilter:gwmilter /app/gnupg && \
    chmod 700 /app/gnupg

# Create keys directory structure
RUN mkdir -p /app/keys/public /app/keys/private && \
    chown -R gwmilter:gwmilter /app/keys && \
    chmod 700 /app/keys/private && \
    chmod 755 /app/keys/public

# Copy key generation script
COPY --chown=root:root --chmod=755 integrations/key-generator/key-generator.sh /app/

# Set the working directory
WORKDIR /app

# Set the environment variables
ENV GNUPGHOME=/app/gnupg
ENV KEY_LIST_FILE=/app/key_list.txt
ENV GWMILTER_USER=gwmilter
ENV GWMILTER_GROUP=gwmilter

# Default command runs the key generation script
CMD ["/app/key-generator.sh"] 