# key-generator service

This service generates PGP keys for testing purposes in the gwmilter project.

## Purpose

The key-generator service is a bootstrap service that:

1. Generates a signing-only key used internally by gwmilter for signing emails
2. Creates public/private key pairs for email addresses listed in [`key_list.txt`](../key_list.txt)
3. Exports all keys to `/app/keys`, which can be a mounted volume used for sharing the keys with the host or other services

## Usage

The service is typically run as a bootstrap step before starting other services:

```yaml
services:
  key-generator:
    build:
      dockerfile: integrations/key-generator/Dockerfile
    volumes:
      - ./keys:/app/keys:rw
      - ./key_list.txt:/app/key_list.txt:ro
    environment:
      - SIGNING_KEY_NAME=gwmilter-signing-key
```

## Environment Variables

- `SIGNING_KEY_NAME`: Name of the signing key (default: `gwmilter-signing-key`)
- `KEY_LIST_FILE`: Path to key list file (default: `/app/key_list.txt`)
- `GWMILTER_USER`: User to run as (default: `gwmilter`)
- `GWMILTER_GROUP`: Group to run as (default: `gwmilter`)

## Output

The service creates:
- `/app/keys/private/*.pgp`: Private keys for all identities
- `/app/keys/public/*.pgp`: Public keys for non-signing-only identities
- `/app/keys/.keys_generated`: Success flag file indicating completion
