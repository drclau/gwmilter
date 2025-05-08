#!/bin/sh

set -euf

die() {
    printf 'Error: %s\n' "$*" >&2
    exit 1
}

CONFIG_FILE_TEMPLATE="${CONFIG_FILE_TEMPLATE:-/app/config.ini.template}"
CONFIG_FILE="${CONFIG_FILE:-/app/config.ini}"
KEYS_DIR="${KEYS_DIR:-/app/keys}"
SIGNING_KEY_NAME="${SIGNING_KEY_NAME:-gwmilter-internal-signing-key}"
GNUPGHOME="${GNUPGHOME:-/app/gnupg}"
GWMILTER_USER="${GWMILTER_USER:-gwmilter}"
GWMILTER_GROUP="${GWMILTER_GROUP:-gwmilter}"
CLEAN_PGP_KEYS_ON_START="${CLEAN_PGP_KEYS_ON_START:-false}"
PUBLIC_KEY_PATTERN="${PUBLIC_KEY_PATTERN:-*.pgp}"

if [ "${CLEAN_PGP_KEYS_ON_START}" = "true" ]; then
    printf 'CLEAN_PGP_KEYS_ON_START is set to true, cleaning %s directory...\n' "$GNUPGHOME"
    if [ -d "$GNUPGHOME" ]; then
        rm -rf "$GNUPGHOME"/* "$GNUPGHOME"/.[!.]* || :
        printf 'PGP keys directory cleaned successfully.\n'
    fi
fi

if ! id -u "$GWMILTER_USER" > /dev/null 2>&1; then
    die "The user '$GWMILTER_USER' does not exist. Cannot continue."
fi
if ! getent group "$GWMILTER_GROUP" > /dev/null 2>&1; then
    die "The group '$GWMILTER_GROUP' does not exist. Cannot continue."
fi

# Configure dirmngr to use the key-server container for key fetching
printf 'Configuring GnuPG dirmngr to use keyserver...\n'
cat > "$GNUPGHOME/dirmngr.conf" << EOF
keyserver hkp://key-server:11371
EOF
chown "$GWMILTER_USER:$GWMILTER_GROUP" "$GNUPGHOME/dirmngr.conf"
chmod 600 "$GNUPGHOME/dirmngr.conf"
printf 'dirmngr keyserver configuration completed.\n'

# Check if the key-generator has bootstrapped the keys
SUCCESS_FLAG="${SUCCESS_FLAG:-${KEYS_DIR}/.keys_generated}"
if [ ! -f "$SUCCESS_FLAG" ]; then
    die "Key generator success flag not found at $SUCCESS_FLAG. Please run the key-generator service first."
else
    printf "Found key generator success flag: %s\n" "$(cat "$SUCCESS_FLAG")"
fi

# Import the private key used by gwmilter internally to sign emails before re-injecting them into the MTA
PRIVATE_KEY_FILE="${KEYS_DIR}/private/${SIGNING_KEY_NAME}.pgp"
if [ -f "$PRIVATE_KEY_FILE" ]; then
    printf 'Importing GnuPG signing key from %s...\n' "$PRIVATE_KEY_FILE"
    if su -s /bin/sh -m "$GWMILTER_USER" -c "gpg --batch --import \"$PRIVATE_KEY_FILE\""; then
        printf 'The GnuPG signing key has been imported successfully.\n'
    else
        die 'Failed to import GnuPG signing key, aborting.'
    fi
else
    die "Required key file not found at $PRIVATE_KEY_FILE. Please run the key-generator bootstrap service first."
fi

printf 'Importing public keys from %s ...\n' "$KEYS_DIR/public"
PUBLIC_KEY_FILES=$(find "$KEYS_DIR/public" -type f -name "${PUBLIC_KEY_PATTERN}")
if [ -z "$PUBLIC_KEY_FILES" ]; then
    printf 'Warning: No public keys found matching pattern %s\n' "${PUBLIC_KEY_PATTERN}"
else
    PUBLIC_KEY_COUNT=0
    for KEY_FILE in $PUBLIC_KEY_FILES; do
        printf 'Importing public key from %s ...\n' "$KEY_FILE"
        if su -s /bin/sh -m "$GWMILTER_USER" -c "gpg --import \"$KEY_FILE\""; then
            PUBLIC_KEY_COUNT=$((PUBLIC_KEY_COUNT + 1))
        else
            printf 'Warning: Failed to import public key from %s\n' "$KEY_FILE"
        fi
    done
    printf 'Imported %s public keys.\n' "$PUBLIC_KEY_COUNT"
fi

# Verify imported GPG keys
printf 'Verifying GPG keyring...\n'
if ! su -s /bin/sh -m "$GWMILTER_USER" -c 'gpg --list-keys > /dev/null'; then
    die 'GPG keyring verification failed.'
fi
printf 'GPG keyring healthy.\n'

#
# Apply environment variable substitution to the config file
printf 'Applying environment variable substitution to config file...\n'
# If missing, default to values suitable for e2e testing setup (i.e. all services running in docker compose)
export MILTER_SOCKET="${MILTER_SOCKET:-inet:10025@0.0.0.0}"
export SMTP_SERVER="${SMTP_SERVER:-smtp://postfix-gw:25}"
export SIGNING_KEY_NAME
envsubst '${MILTER_SOCKET} ${SMTP_SERVER} ${SIGNING_KEY_NAME}' < "$CONFIG_FILE_TEMPLATE" > "${CONFIG_FILE}"

exec /app/gwmilter -c "$CONFIG_FILE"
