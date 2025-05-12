# PGP Key Server

A lightweight PGP public key server implementing the HKP (HTTP Keyserver Protocol).
This is meant for testing only and should not be used in production.

## Features

- Serves PGP public keys from a filesystem directory.
- Implements HKP (HTTP Keyserver Protocol) for compatibility with PGP clients.
- RESTful API for key lookup and management.
- Keys are stored as files following the naming pattern `<email>.pgp` (e.g., `user@example.com.pgp`).

## API Endpoints

### HKP (HTTP Keyserver Protocol) Endpoints

- `/pks/lookup?op=get&search=user@example.com` - Get a specific key by email.
- `/pks/lookup?op=index&search=example.com` - Search for keys matching a string.

### RESTful API Endpoints

- `GET /keys` - List all available keys.
- `GET /keys/{email}` - Get a specific key by email.

## Configuration

The service can be configured using environment variables:

- `KEYS_DIR`: Path to the directory containing public keys (default: `/app/keys/public`).
- `HOST`: Host to bind the server to (default: `0.0.0.0`).
- `PORT`: Port to run the server on (default: `11371`).
- `LOG_LEVEL`: Sets the logging level (default: `INFO`).

## Usage

The service is integrated with **docker compose** setups but can also be run independently via Docker or on the host:

```sh
# via docker
% docker run --rm -it --name keyserver -p 11371:11371 -v $(pwd)/integrations/keys/public:/app/keys/public:ro keyserver:latest

# directly on host
% KEYS_DIR=$(pwd)/integrations/keys/public GNUPGHOME=$(pwd)/integrations/gnupg python integrations/key-server/app.py

% curl http://localhost:11371/keys|jq
{
  "keys": [
    {
      "email": "user1-pgp@example.com",
      "url": "/keys/user1-pgp@example.com",
      "keyid": "E67C892B00B7CF70",
      "fingerprint": "B9A9713469E054DD7A7D581FE67C892B00B7CF70",
      "created": "1747053864",
      "expires": "1778589864",
      "algo": "22",
      "keylen": "256",
      "flags": "",
      "uids": [
        "user1-pgp@example.com <user1-pgp@example.com>"
      ]
    },
    {
      "email": "user2-pgp@example.com",
      "url": "/keys/user2-pgp@example.com",
      "keyid": "EC0964712F49313D",
      "fingerprint": "F169679088666E6F75074637EC0964712F49313D",
      "created": "1747053864",
      "expires": "1747053865",
      "algo": "22",
      "keylen": "256",
      "flags": "e",
      "uids": [
        "user2-pgp@example.com <user2-pgp@example.com>"
      ]
    }
  ]
}

% curl --http1.0 'http://localhost:11371/pks/lookup?op=index&options=mr&fingerprint=on&search=user1-pgp@example.com'
info:1:1
pub:B9A9713469E054DD7A7D581FE67C892B00B7CF70:22:256:1747053864:1778589864:
uid:user1-pgp@example.com%20%3Cuser1-pgp@example.com%3E:1747053864:1778589864:

% curl 'http://localhost:11371/pks/lookup?op=get&search=user1-pgp@example.com'
-----BEGIN PGP PUBLIC KEY BLOCK-----

<public key content>

-----END PGP PUBLIC KEY BLOCK-----
```