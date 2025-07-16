ARG ALPINE_VERSION=3.22

FROM alpine:${ALPINE_VERSION}

# Install test runtime dependencies
RUN apk add --no-cache \
    gnupg \
    python3 \
    py3-pip \
    docker-cli

# Create virtual environment for Python dependencies
RUN python3 -m venv /venv

# Activate virtual environment and upgrade pip
RUN . /venv/bin/activate && pip install --upgrade pip

# Copy test requirements to a temporary location for installation
COPY tests/requirements.txt /tmp/requirements.txt

# Install test requirements
RUN . /venv/bin/activate && pip install -r /tmp/requirements.txt

# Copy entrypoint script
COPY --chown=root:root --chmod=755 integrations/tests-runner/entrypoint.sh /app/entrypoint.sh

# Set working directory
WORKDIR /

# Use entrypoint script
ENTRYPOINT ["/app/entrypoint.sh"] 