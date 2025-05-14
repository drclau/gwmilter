#!/bin/sh
# Unified PGP utility script for gwmilter
# Usage: ./pgp-utils.sh [--local | --container <container>] <command> [OPTIONS]

set -eu

# Configuration
GNUPGHOME="${GNUPGHOME:-/app/gnupg}"
GWMILTER_USER="${GWMILTER_USER:-gwmilter}"

# Error handling
die() {
    printf 'Error: %s\n' "$*" >&2
    exit 1
}

# Validate docker and container presence
validate_docker_container() {
    if ! command -v docker >/dev/null 2>&1; then
        die "docker CLI not found. Please install Docker."
    fi
    if ! docker container inspect "${GWMILTER_CONTAINER}" >/dev/null 2>&1; then
        die "Container '${GWMILTER_CONTAINER}' not found."
    fi
    if [ "$(docker container inspect -f '{{.State.Running}}' "${GWMILTER_CONTAINER}")" != "true" ]; then
        die "Container '${GWMILTER_CONTAINER}' is not running."
    fi
}

# Execute commands based on run mode
run() {
    echo command: "$@"
    case "${GWMILTER_RUN_MODE}" in
        local)
            if [ "$(id -u)" -eq 0 ] && [ -n "${GWMILTER_USER}" ] && [ "${GWMILTER_USER}" != "root" ]; then
                # Write the command to a temporary script
                umask 022
                tmp_script=$(mktemp)
                printf '#!/bin/sh\nexec ' > "$tmp_script"
                for arg; do
                    printf " '%s'" "$(printf "%s" "$arg" | sed "s/'/'\\\\''/g")" >> "$tmp_script"
                done
                printf '\n' >> "$tmp_script"
                chmod 755 "$tmp_script"
                su -s /bin/sh -m "${GWMILTER_USER}" -c "$tmp_script"
                rm -f "$tmp_script"
            else
                "$@"
            fi
            ;;
        docker)
            docker exec -u "${GWMILTER_USER}" "${GWMILTER_CONTAINER}" "$@"
            ;;
        *)
            die "Unknown GWMILTER_RUN_MODE: ${GWMILTER_RUN_MODE}"
            ;;
    esac
}

# ===== DELETE KEYS FUNCTIONALITY =====

# Helper to filter colon-separated GPG output and return matching key IDs
filter_keyinfo() {
    awk -v pattern="${KEY_PATTERN}" -F: '
        BEGIN { IGNORECASE = 1 }
        /^pub/ { pubkey = $5; match_found = 0 }
        /^fpr/ { if ($10 ~ pattern) match_found = 1 }
        /^uid/ { if ($10 ~ pattern) match_found = 1 }
        match_found == 1 && pubkey != "" {
            print pubkey
            pubkey = ""
            match_found = 0
        }
    '
}

# Mode-dependent key retrieval function
get_keys() {
    run gpg --list-keys --with-colons | filter_keyinfo
}

# Function to list keys
list_keys() {
    printf "Listing all public keys:\n"
    run gpg --list-keys
}

# Function to confirm deletion
confirm_deletion() {
    if [ "${FORCE_DELETE:-0}" -eq 0 ]; then
        printf "Are you sure you want to proceed? [y/N] "
        read -r CONFIRM
        if [ "${CONFIRM}" != "y" ] && [ "${CONFIRM}" != "Y" ]; then
            printf "Deletion cancelled.\n"
            exit 0
        fi
    fi
}

# Delete keys command
cmd_delete_keys() {
    # Parse command line options
    LIST_KEYS=0
    FORCE_DELETE=0
    
    OPTIND=1
    while getopts "lfh" opt; do
        case "${opt}" in
            l) LIST_KEYS=1 ;;
            f) FORCE_DELETE=1 ;;
            h) usage_delete_keys; exit 0 ;;
            *) usage_delete_keys; exit 1 ;;
        esac
    done

    shift $((OPTIND-1))

    if [ $# -lt 1 ]; then
        die "Error: Key pattern required"
    fi

    # Prevent pattern starting with '-' being misinterpreted as an option
    if [ "${1#-}" != "$1" ]; then
        die "Error: key_pattern '$1' looks like an option. Please separate options and pattern with '--'"
    fi

    KEY_PATTERN=$1

    printf "Running in %s mode\n" "${GWMILTER_RUN_MODE}"

    # Set the GPG home directory for local execution
    if [ "${GWMILTER_RUN_MODE}" = "local" ]; then
        # Check for the required user
        if ! id -u "${GWMILTER_USER}" > /dev/null 2>&1; then
            printf "Warning: The user '%s' does not exist. Running as current user.\n" "${GWMILTER_USER}"
            GWMILTER_USER=$(whoami)
        fi
    fi

    # List keys if requested
    if [ "${LIST_KEYS:-0}" -eq 1 ]; then
        list_keys
    fi

    # Get matching keys
    MATCHING_KEYS=$(get_keys)

    # Check if any keys match the pattern
    if [ -z "${MATCHING_KEYS}" ]; then
        printf "No keys found matching pattern: %s\n" "${KEY_PATTERN}"
        printf "DELETED_KEYS_COUNT=0\n"
        exit 0
    fi

    printf "Found the following keys matching '%s':\n" "${KEY_PATTERN}"
    for KEY_ID in ${MATCHING_KEYS}; do
        run gpg --list-keys "${KEY_ID}"
    done

    confirm_deletion

    # Delete the keys
    DELETION_SUCCESS=0
    DELETION_FAILURE=0
    printf "Deleting keys matching: %s...\n" "${KEY_PATTERN}"
    for KEY_ID in ${MATCHING_KEYS}; do
        printf "Deleting key: %s\n" "${KEY_ID}"
        if run gpg --batch --yes --delete-key "${KEY_ID}"; then
            DELETION_SUCCESS=$((DELETION_SUCCESS + 1))
        else
            printf "Warning: Failed to delete key %s\n" "${KEY_ID}"
            DELETION_FAILURE=$((DELETION_FAILURE + 1))
        fi
    done

    # Report results
    printf "\nDeletion complete: %d keys deleted, %d failed.\n" "${DELETION_SUCCESS}" "${DELETION_FAILURE}"
    if [ "${DELETION_FAILURE}" -gt 0 ]; then
        printf "Some keys could not be deleted. Make sure you have the correct permissions.\n"
    fi

    # List keys after deletion if requested
    if [ "${LIST_KEYS:-0}" -eq 1 ]; then
        printf "\n"
        list_keys
    fi

    # Output the number of deleted keys for programmatic use
    printf "DELETED_KEYS_COUNT=%d\n" "${DELETION_SUCCESS}"
}

# ===== RESET KEYSERVER FUNCTIONALITY =====

# Reset keyserver command
cmd_reset_keyserver() {
    printf "Running in %s mode\n" "${GWMILTER_RUN_MODE}"
    
    printf "Resetting keyserver to alive status in GPG dirmngr...\n"
    run gpg-connect-agent --dirmngr 'KEYSERVER --alive key-server' /bye
    printf "Keyserver reset to alive status in GPG dirmngr.\n"
}

# ===== USAGE FUNCTIONS =====

usage_delete_keys() {
    cat << EOF
Usage: $0 [--local | --container <container>] delete-keys [OPTIONS] <key_pattern>

Delete PGP public keys from the specified gwmilter container or host machine

Options:
  -l, --list           List all public keys before and after deletion
  -f, --force          Force deletion without confirmation
  -h, --help           Display this help message

Arguments:
  key_pattern          Regex pattern to match email address or key fingerprint

Examples:
  $0 --container integrations-gwmilter-1 delete-keys user@example.com      # Delete key by exact email
  $0 --container integrations-gwmilter-1 delete-keys ABC123DE456F78901234  # Delete key by fingerprint
  $0 --local delete-keys 'pgp-.*@example.com'                              # Delete keys locally
  $0 --container integrations-gwmilter-1 delete-keys -l user               # List and delete keys
EOF
}

usage_reset_keyserver() {
    cat << EOF
Usage: $0 [--local | --container <container>] reset-keyserver

Reset the keyserver to alive status in GPG dirmngr

Examples:
  $0 --container integrations-gwmilter-1 reset-keyserver    # Reset keyserver in container
  $0 --local reset-keyserver                                # Reset keyserver locally
EOF
}

usage() {
    cat << EOF
Unified PGP utility script for gwmilter
Usage: $0 [--local | --container <container>] <command> [OPTIONS]

Global Options:
  --local                  Run commands locally (not in Docker)
  --container <container>  Run commands in the specified container

Commands:
  delete-keys [OPTIONS] <key_pattern>    Delete PGP keys matching pattern
  reset-keyserver                        Reset keyserver to alive status
  help                                   Display this help message

Environment variables:
  GNUPGHOME            GPG home directory (default: /app/gnupg)
  GWMILTER_USER        User to run as (default: gwmilter)

Run '$0 help <command>' for more information on a specific command
EOF
}

# ===== MAIN SCRIPT EXECUTION BEGINS HERE =====

if [ $# -lt 1 ]; then
    usage
    exit 1
fi

# Parse global options
GWMILTER_CONTAINER=""
MODE_SET=0

while [ $# -gt 0 ]; do
    case "$1" in
        --local)
            GWMILTER_RUN_MODE="local"
            MODE_SET=1
            shift 1
            ;;
        --container)
            if [ $# -lt 2 ]; then
                die "Error: --container requires a container name"
            fi
            GWMILTER_CONTAINER="$2"
            GWMILTER_RUN_MODE="docker"
            MODE_SET=1
            shift 2
            validate_docker_container
            ;;
        help)
            # Special case for help command without mode
            if [ $# -eq 1 ]; then
                usage
            else
                case "$2" in
                    delete-keys) usage_delete_keys ;;
                    reset-keyserver) usage_reset_keyserver ;;
                    *) usage ;;
                esac
            fi
            exit 0
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -* )
            die "Error: Unknown option $1"
            ;;
        *)
            break
            ;;
    esac
done

if [ $MODE_SET -eq 0 ]; then
    die "You must specify --local or --container <container> before the command."
fi

if [ $# -lt 1 ]; then
    usage
    exit 1
fi

COMMAND="$1"
shift 1

case "${COMMAND}" in
    delete-keys)
        cmd_delete_keys "$@"
        ;;
    reset-keyserver)
        cmd_reset_keyserver "$@"
        ;;
    *)
        printf "Unknown command: %s\n" "${COMMAND}"
        usage
        exit 1
        ;;
esac

exit 0 