#!/bin/sh
set -eu

echo "Starting postfix"

touch /etc/postfix/aliases
postalias /etc/postfix/aliases

echo "Applying environment variable substitution to config file..."
# If missing, default to values suitable for e2e testing setup (i.e. all services running in docker compose)
export SMTPD_MILTERS="${SMTPD_MILTERS:=inet:gwmilter:10025}"
export RELAYHOST="${RELAYHOST:=[mailpit]:1025}"
echo "SMTPD_MILTERS: ${SMTPD_MILTERS}"
echo "RELAYHOST: ${RELAYHOST}"
# shellcheck disable=SC2016
envsubst '${SMTPD_MILTERS} ${RELAYHOST}' < /etc/postfix/main.cf.template > /etc/postfix/main.cf

exec postfix start-fg
