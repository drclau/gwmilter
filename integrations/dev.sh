#!/bin/sh

# This script prepares the gwmilter configuration file for local development,
# You can start gwmilter with the following command:
#
# ./build/gwmilter -c dev-config.ini
#
# where "./build" is the path to the build directory.

set -eu

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG_FILE_TEMPLATE="${CONFIG_FILE_TEMPLATE:-$PROJECT_ROOT/integrations/gwmilter/config.ini.template}"
CONFIG_FILE="${CONFIG_FILE:-$PROJECT_ROOT/dev-config.ini}"
GNUPGHOME="${GNUPGHOME:-$HOME/.gnupg}"
DIRMNGR_KEYSERVER_ADDR="hkp://localhost:11371"

usage() {
    cat <<EOF
Usage: $0 [options]

Prepares the gwmilter configuration file for local development.

Options:
  -h, --help    Show this help message and exit

Environment variables:
  CONFIG_FILE_TEMPLATE   Path to the config template (default: integrations/gwmilter/config.ini.template)
  CONFIG_FILE            Path to the config file to generate (default: dev-config.ini)
  MILTER_SOCKET          Milter socket (default: inet:10025@0.0.0.0)
  SMTP_SERVER            SMTP server URL (default: smtp://localhost:25)
  SIGNING_KEY_NAME       Signing key name (default: gwmilter-signing-key)
EOF
}

# Check for help flag
if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
    usage
    exit 0
fi

# Print a path relative to the current working directory, or absolute if not possible
relative_to_pwd() {
    target="$1"
    abs_target=$(realpath "$target")
    abs_pwd=$(realpath "$PWD")
    rel_path="${abs_target#"$abs_pwd"/}"
    if [ "$rel_path" = "$abs_target" ]; then
        # Not a subpath, print absolute
        printf '%s' "$abs_target"
    else
        printf '%s' "$rel_path"
    fi
}

# Helper function for prompts with validation and default (POSIX sh compatible)
prompt_with_default() {
    prompt="$1"
    default="$2"
    valid="$3" # e.g. 'y n' or 'f b'
    while true; do
        printf "%s" "$prompt" 1>&2
        read -r input
        [ -z "$input" ] && input="$default"
        for v in $valid; do
            if [ "$input" = "$v" ]; then
                PROMPT_RESULT="$input"
                return
            fi
        done
        printf 'Please enter one of: %s.\n' "$valid" 1>&2
    done
}

# Helper function to generate the config file with envsubst
# Usage: generate_config OUTPUT_FILE
generate_config() {
    # shellcheck disable=SC2016
    envsubst '${MILTER_SOCKET} ${SMTP_SERVER} ${SIGNING_KEY_NAME}' < "$CONFIG_FILE_TEMPLATE" > "$1"
}

# Helper function to check dirmngr.conf
check_dirmngr_config() {
    DIRMNGR_CONF="$GNUPGHOME/dirmngr.conf"
    target_keyserver="keyserver $DIRMNGR_KEYSERVER_ADDR"
    error_msg=""

    if [ ! -f "$DIRMNGR_CONF" ]; then
        # Use the direct path in the error message as relative_to_pwd would fail
        error_msg=$(printf '\n\033[33mWarning:\033[0m Configuration file \033[1m%s\033[0m is missing.' "$DIRMNGR_CONF")
    else
        # File exists, now we can safely get the relative path
        relative_dirmngr_conf=$(relative_to_pwd "$DIRMNGR_CONF")
        first_keyserver=$(grep -m 1 '^keyserver ' "$DIRMNGR_CONF" || true)
        if [ "$first_keyserver" != "$target_keyserver" ]; then
            error_msg=$(printf '\n\033[33mWarning:\033[0m Your \033[1m%s\033[0m does not have the required \033[3mkeyserver\033[0m as the first entry.' "$relative_dirmngr_conf")
        fi
    fi

    if [ -n "$error_msg" ]; then
        printf '%s\n' "$error_msg"
        printf 'Please ensure the following line is the *first* \033[3mkeyserver\033[0m entry:\n\n'
        printf '\033[3m%s\033[0m\n\n' "$target_keyserver"
        printf 'Incorrect configuration may cause some tests to fail.\n'
    fi
}

printf 'Preparing \033[3mgwmilter\033[0m configuration file for local development...\n'
# If missing, default to values suitable for e2e testing setup (i.e. all services running in docker compose)
export MILTER_SOCKET="${MILTER_SOCKET:-inet:10025@0.0.0.0}"
export SMTP_SERVER="${SMTP_SERVER:-smtp://localhost:25}"
export SIGNING_KEY_NAME="${SIGNING_KEY_NAME:-gwmilter-signing-key}"

if [ -f "$CONFIG_FILE" ]; then
    relative_config_file="$(relative_to_pwd "$CONFIG_FILE")"
    while true; do
        prompt=$(printf "Configuration file \033[1m%s\033[0m already exists, overwrite? (y/n/d) [n]: " "$relative_config_file")
        prompt_with_default "$prompt" n "y n d"
        OVERWRITE="$PROMPT_RESULT"
        if [ "$OVERWRITE" = "y" ]; then
            generate_config "$CONFIG_FILE"
            # shellcheck disable=SC3003
            printf "gwmilter configuration file prepared at %b%s%b\n" $'\033[1m' "$relative_config_file" $'\033[0m'
            break
        elif [ "$OVERWRITE" = "n" ]; then
            # shellcheck disable=SC3003
            printf "Using existing configuration file at %b%s%b\n" $'\033[1m' "$relative_config_file" $'\033[0m'
            break
        elif [ "$OVERWRITE" = "d" ]; then
            TMP_NEW_CONFIG=$(mktemp)
            generate_config "$TMP_NEW_CONFIG"
            if diff -q "$CONFIG_FILE" "$TMP_NEW_CONFIG" >/dev/null; then
                printf '\n\033[3mNo differences found between the existing and new config.\033[0m\n' 1>&2
                break
            else
                printf '\n--- Diff (existing vs. new config) ---\n' 1>&2
                if command -v colordiff >/dev/null 2>&1; then
                    colordiff -u "$CONFIG_FILE" "$TMP_NEW_CONFIG" 1>&2 || true
                else
                    diff -u "$CONFIG_FILE" "$TMP_NEW_CONFIG" 1>&2 || true
                fi
                printf '\n--- End of diff ---\n' 1>&2
            fi
            rm -f "$TMP_NEW_CONFIG"
        fi
    done
else
    generate_config "$CONFIG_FILE"
    # shellcheck disable=SC3003
    printf "gwmilter configuration file prepared at %b%s%b\n" $'\033[1m' "$(relative_to_pwd "$CONFIG_FILE")" $'\033[0m'
fi

# Check dirmngr.conf configuration
check_dirmngr_config

printf '\nYou can start the docker compose stack with the following command:\n'
printf '\033[1mdocker compose -f %s up\033[0m\n' "$(relative_to_pwd "$PROJECT_ROOT/integrations/docker-compose-no-gwmilter.yaml")"
