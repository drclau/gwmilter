#!/bin/sh
# Wrapper for configuring libegpgcrypt and libepdfcrypt
#
# Ensures ./configure always runs to generate fresh Makefiles, even when
# vendored sources include pre-generated configure scripts. Runs ./bootstrap
# only if configure doesn't exist.

if [ ! -x ./configure ]; then
    ./bootstrap
fi

exec ./configure "$@"
