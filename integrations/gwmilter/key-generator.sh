#!/bin/sh
# PGP key generator for test environment
#
# Creates one signing-only key and public/private key pairs based on
# key_list.txt. The signing-only key is used internally by gwmilter for
# signing emails that are re-injected into the MTA.
#
# This script is intended to be run as a bootstrapping service.
# It generates keys and exports them to a mounted directory.

set -eu
umask 077

# Required variables and configuration
KEY_LIST_FILE=${KEY_LIST_FILE:-"/app/key_list.txt"}
GWMILTER_USER=${GWMILTER_USER:-"gwmilter"}
GWMILTER_GROUP=${GWMILTER_GROUP:-"gwmilter"}
# Flag file to indicate successful completion
SUCCESS_FLAG="/app/keys/.keys_generated"

die() {
    printf 'Error: %s\n' "$*" >&2
    exit 1
}

# Helper function to run commands as gwmilter user
run_as_gwmilter() {
    su -s /bin/sh -m "$GWMILTER_USER" -c "$@"
}

# Setup function
setup_keys_directory() {
    mkdir -p /app/keys/public
    mkdir -p /app/keys/private
    chown -R "$GWMILTER_USER":"$GWMILTER_GROUP" /app/keys
    chmod 700 /app/keys/private
    chmod 755 /app/keys/public
}

# Generate key function
generate_key() {
    key_type=$1
    identity=$2
    expiration=$3  # Optional expiration parameter

    printf "Creating key for %s...\n" "$identity"

    if [ "$key_type" = "signing-only" ]; then
        # Generate signing-only signing key (no expiration for private keys)
        run_as_gwmilter "gpg --batch --passphrase '' --pinentry-mode loopback --quick-gen-key \"$identity\" default sign never"
        run_as_gwmilter "gpg --batch --export-secret-keys --armor --output /app/keys/private/${identity}.pgp \"$identity\""
        printf "✅ signing-only key exported to /app/keys/private/%s.pgp\n" "$identity"
    elif [ "$key_type" = "keypair" ]; then
        # Use expiration if provided, otherwise default to 1y
        if [ -z "$expiration" ]; then
            # Default to 1y if not specified
            expiration="1y"
        fi

        # Generate key pair for $identity with the specified expiration
        run_as_gwmilter "gpg --batch --gen-key <<EOF
%echo Generating a new ed25519/cv25519 key pair
Key-Type: eddsa
Key-Curve: ed25519
Key-Usage: sign,cert
Subkey-Type: ecdh
Subkey-Curve: cv25519
Subkey-Usage: encrypt
Name-Real: $identity
Name-Email: $identity
Expire-Date: $expiration
%no-protection
%commit
%echo Done
EOF"
        # Export public key
        run_as_gwmilter "gpg --batch --export --armor --output /app/keys/public/${identity}.pgp \"$identity\""
        # Export private key
        run_as_gwmilter "gpg --batch --export-secret-keys --armor --output /app/keys/private/${identity}.pgp \"$identity\""
        printf "✅ Key pair exported to /app/keys/public/%s.pgp and /app/keys/private/%s.pgp\n" "$identity" "$identity"
    else
        printf "❌ ERROR: Invalid key type: %s\n" "$key_type" >&2
        exit 1
    fi
}

# Validate key list
validate_key_list() {
    if [ ! -f "$KEY_LIST_FILE" ]; then
        printf "❌ ERROR: Key list file not found: %s\n" "$KEY_LIST_FILE" >&2
        printf "Please create the key list file with email addresses.\n" >&2
        exit 1
    fi

    printf "Processing key list file: %s\n" "$KEY_LIST_FILE"

    # Count valid (non-empty, non-comment) lines
    valid_entries=$(grep -cv "^[[:space:]]*\(#\|$\)" "$KEY_LIST_FILE")

    if [ "$valid_entries" -eq 0 ]; then
        printf "❌ ERROR: Key list file is empty or contains only comments: %s\n" "$KEY_LIST_FILE" >&2
        printf "Please add at least one valid email address\n" >&2
        exit 1
    fi

    printf "Found %s valid entries in %s\n" "$valid_entries" "$KEY_LIST_FILE"
}

# Print summary function
print_summary() {
    printf "\n"
    printf "=== All keys generated successfully ===\n"

    # Create success flag file
    touch "$SUCCESS_FLAG"
    printf "%s: Key generation completed successfully\n" "$(date)" > "$SUCCESS_FLAG"
    chmod 644 "$SUCCESS_FLAG"
    printf "Created success flag file: %s\n" "$SUCCESS_FLAG"

    # Kill all GPG related daemons to ensure clean exit
    printf "Terminating GPG daemons...\n"
    run_as_gwmilter "gpgconf --kill all"
    printf "GPG daemons terminated.\n"
}


###############################################################################
#                                                                             #
#                           === MAIN EXECUTION ===                            #
#                                                                             #
###############################################################################

# Check if keys were already generated successfully
if [ -f "$SUCCESS_FLAG" ]; then
    printf "Found existing success flag at %s\n" "$SUCCESS_FLAG"
    printf "Content: %s\n" "$(cat "$SUCCESS_FLAG")"
    printf "Skipping key generation as keys were already generated successfully.\n"
    printf "Delete this flag file if you want to regenerate keys.\n"
    exit 0
fi

run_as_gwmilter 'command -v gpg > /dev/null 2>&1' || die 'gpg not installed or not in PATH for gwmilter user'
setup_keys_directory
validate_key_list

printf "=== Generating GPG keys for testing ===\n"

# Always generate signing-only key matching the signing key from config.ini
generate_key "signing-only" "$SIGNING_KEY_NAME" "never"

# Generate public/private key pairs from key list
printf "Generating key pairs from %s\n" "$KEY_LIST_FILE"
while IFS= read -r line || [ -n "$line" ]; do
    # Skip comments and empty lines
    case "$line" in
        \#*)
            # Comment line
            continue
            ;;
        *)
            # Check if line contains only whitespace
            if [ -z "$(echo "$line" | tr -d '[:space:]')" ]; then
                continue
            fi
            ;;
    esac

    # Parse the line using case statement (POSIX-compliant)
    case "$line" in
        *:*)
            # Has expiration
            key_email=${line%%:*}
            expiration=${line#*:}
            ;;
        *)
            # No expiration
            key_email=$line
            expiration=""
            ;;
    esac

    if [ -n "$key_email" ]; then
        generate_key "keypair" "$key_email" "$expiration"
    else
        printf "⚠️ Invalid line format: %s\n" "$line" >&2
        printf "Skipping this entry...\n" >&2
    fi
done < "$KEY_LIST_FILE"

print_summary