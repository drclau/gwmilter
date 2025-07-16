#!/bin/sh

set -euf

# Activate Python virtual environment
. /venv/bin/activate

# Run pytest with configured options
exec pytest \
    -o console_output_style=classic \
    --gwmilter-container=integrations-gwmilter-1 \
    --smtp-host=postfix-gw \
    --smtp-port=25 \
    --mailpit-url=http://mailpit:8025 \
    --keys-dir=/app/keys \
    tests 