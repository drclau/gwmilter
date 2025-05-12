#!/usr/bin/env python3

import logging
import os
import re
import tempfile
import urllib.parse
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional

import gnupg
import uvicorn
from fastapi import FastAPI, HTTPException, Query, Response
from fastapi import Path as PathParam
from fastapi.responses import PlainTextResponse

# Configure logging
LOG_LEVEL = os.environ.get("LOG_LEVEL", "INFO").upper()
numeric_level = getattr(logging, LOG_LEVEL, logging.INFO)
logging.basicConfig(
    level=numeric_level,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
)
logger = logging.getLogger("sks-service")
logger.info(f"Logging configured at {LOG_LEVEL} level")

app = FastAPI(title="PGP Key Server")

# Configuration
KEYS_DIR = os.environ.get("KEYS_DIR", "/app/keys/public")
HOST = os.environ.get("HOST", "0.0.0.0")
PORT = int(os.environ.get("PORT", "11371"))

# Set up GnuPG instance
GPG_HOME = os.environ.get("GNUPGHOME", "/app/gnupg")
logger.info(f"Using GnuPG home directory: {GPG_HOME}")
gpg = gnupg.GPG(gnupghome=GPG_HOME)
gpg.encoding = "utf-8"


def sanitize_email(email: str) -> str:
    """
    Allow only valid email characters and prevent path traversal.
    """
    # Only allow letters, digits, @, ., _, -, +
    if not re.fullmatch(r"[A-Za-z0-9_.+\-@]+", email):
        raise HTTPException(status_code=400, detail="Invalid email format")
    return email.lower()


def get_key_by_email(email: str) -> Optional[str]:
    """
    Retrieve a PGP key for the given email address.
    """
    email = sanitize_email(email)
    key_path = Path(KEYS_DIR) / f"{email}.pgp"
    if not key_path.exists():
        return None

    try:
        with open(key_path, "r") as f:
            return f.read()
    except Exception as e:
        logger.error(f"Error reading key file for {email}: {e}")
        return None


def extract_key_info(key_path: Path) -> Optional[Dict[str, Any]]:
    """
    Extract detailed information from a PGP key file using the gnupg Python package.
    """
    logger.info(f"Extracting key info from {key_path}")
    try:
        # Read the key content
        with open(key_path, "r") as f:
            key_content = f.read()
        logger.debug(f"Successfully read key content from {key_path}")

        # Create a temporary GPG instance to avoid affecting the main keyring
        with tempfile.TemporaryDirectory() as temp_dir:
            logger.debug(f"Created temporary GPG directory at {temp_dir}")
            temp_gpg = gnupg.GPG(gnupghome=temp_dir)
            temp_gpg.encoding = "utf-8"

            # Import the key to get its information
            import_result = temp_gpg.import_keys(key_content)
            logger.debug(f"Key import result: {import_result.count} keys imported")

            if not import_result.fingerprints:
                logger.error(f"Failed to import key from {key_path}")
                return None

            fingerprint = import_result.fingerprints[0]
            logger.debug(f"Extracted fingerprint: {fingerprint}")

            # Get detailed information about the key
            keys = temp_gpg.list_keys(keys=[fingerprint])
            if not keys:
                logger.error(f"Failed to retrieve key details for {fingerprint}")
                return None

            key_data = keys[0]
            logger.debug(f"Key data retrieved: keyid={key_data.get('keyid', 'N/A')}")

            # Extract user IDs
            uids = []
            for uid in key_data.get("uids", []):
                uids.append(uid)
            logger.debug(f"Extracted {len(uids)} user IDs")

            # Determine key algorithm number
            algo_num = key_data.get("algo", 0)  # Default to 0 if missing
            logger.debug(f"Algorithm: {algo_num}")

            # Get key length
            keylen = key_data.get("keylen", "")
            if not keylen:
                # Try to determine key length from type
                if algo_num in ["22", "18"]:
                    keylen = "256"
            logger.debug(f"Key length: {keylen}")

            # Extract creation date (as Unix timestamp)
            created_timestamp = key_data.get("date", "")
            logger.debug(f"Creation timestamp: {created_timestamp}")

            # Extract expiration date (as Unix timestamp)
            expires_timestamp = key_data.get("expires", "")
            logger.debug(f"Expiration timestamp: {expires_timestamp}")

            # Determine flags
            flags = ""
            if expires_timestamp and expires_timestamp != "":
                # Check if key is expired
                try:
                    if int(expires_timestamp) < datetime.now().timestamp():
                        flags += "e"
                        logger.debug("Key is expired, adding 'e' flag")
                except ValueError:
                    logger.warning(f"Invalid expiration timestamp: {expires_timestamp}")

            # Construct the key info object
            key_info = {
                "email": key_path.stem,  # Use filename as primary email
                "filename": key_path.name,
                "keyid": key_data.get("keyid", ""),
                "fingerprint": fingerprint,
                "algo": algo_num,
                "keylen": keylen,
                "created": created_timestamp,
                "expires": expires_timestamp,
                "flags": flags,
                "content": key_content,
                "uids": uids,
            }

            logger.info(
                f"Successfully extracted key info for {key_path.name} (ID: {key_info['keyid']})"
            )
            return key_info

    except Exception as e:
        logger.error(f"Error extracting key info for {key_path}: {e}")
        return None


def list_all_keys() -> List[Dict[str, Any]]:
    """
    List all keys in the public directory with detailed information.
    """
    keys = []
    for key_file in Path(KEYS_DIR).glob("*.pgp"):
        key_info = extract_key_info(key_file)
        if key_info:
            keys.append(key_info)

    return keys


@app.get("/")
def root():
    """
    Root endpoint providing server information.
    This endpoint is used by GPG clients to check server capabilities.
    """
    # Use PlainTextResponse to ensure compatibility with older clients
    return Response(
        content="SKS OpenPGP Public Key Server\n"
        + "Software: sks-service 1.0.0\n"
        + "Supported operations: get, index\n",
        media_type="text/plain",
    )


@app.get("/pks/lookup", response_class=PlainTextResponse)
def lookup(
    op: str = Query(
        ...,
        pattern="^(get|index)$",
        max_length=5,
        description="must be 'get' or 'index'",
    ),
    search: Optional[str] = Query(
        None, min_length=1, max_length=256, description="limited to 256 chars"
    ),
    options: Optional[str] = Query(
        None,
        min_length=1,
        max_length=100,
        description="comma‐separated opts, ≤100 chars",
    ),
    fingerprint: Optional[str] = Query(
        None, pattern="^(on|off)$", max_length=3, description="'on' or 'off'"
    ),
    exact: Optional[str] = Query(
        None, pattern="^(on|off)$", max_length=3, description="'on' or 'off'"
    ),
):
    """
    HKP (HTTP Keyserver Protocol) compatible lookup endpoint.

    Operations:
    - get: Get a specific key by email or key ID
    - index: Search/list keys

    Options:
    - mr: Machine readable format
    - nm: No modify (not implemented)
    - x-<option>: Extensions (ignored)

    Other parameters:
    - fingerprint: Whether to show full fingerprints (on/off)
    - exact: Whether to match exactly (on/off)
    """
    logger.info(
        f"Lookup request: op={op}, search={search}, options={options}, fingerprint={fingerprint}, exact={exact}"
    )

    # Parse options
    option_list = options.split(",") if options else []
    machine_readable = "mr" in option_list

    # Handle exact search
    exact_search = exact == "on"

    if op == "get" and search:
        # Extract email or key ID from the search parameter
        key_data = None
        search_type = ""

        logger.info(f"Key retrieval request: op=get, search={search}")

        # Check if it's a key ID search (starts with 0x)
        if search.startswith("0x") or re.match(r"^[A-Fa-f0-9]{8,40}$", search):
            search_type = "keyid"
            logger.info(f"Key ID/fingerprint search: {search}")
            key_data = get_key_by_email(
                search.lower()
            )  # Try direct filename match first
            if not key_data:
                # Search for key ID in all keys
                for key_file in Path(KEYS_DIR).glob("*.pgp"):
                    key_info = extract_key_info(key_file)
                    if key_info and (
                        search.upper().replace("0X", "")
                        in key_info["fingerprint"].upper()
                        or search.upper().replace("0X", "") in key_info["keyid"].upper()
                    ):
                        with open(key_file, "r") as f:
                            key_data = f.read()
                            break
        else:
            # Try email search
            search_type = "email"
            email_match = re.search(
                r"([a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+)", search
            )
            if email_match:
                email = email_match.group(1)
                logger.info(f"Email search: {email}")
                key_data = get_key_by_email(email)

                # If exact search is enabled, verify that search matches email exactly
                if exact_search and key_data and email.lower() != search.lower():
                    key_data = None

        if key_data:
            logger.info(f"Key found for {search_type} search: {search}")

            # Ensure the key data has proper PGP formatting
            if "-----BEGIN PGP PUBLIC KEY BLOCK-----" not in key_data:
                key_data = f"-----BEGIN PGP PUBLIC KEY BLOCK-----\n\n{key_data}\n-----END PGP PUBLIC KEY BLOCK-----"

            # Set the appropriate content type based on options
            content_type = "text/plain"
            if not machine_readable:
                content_type = "application/pgp-keys"

            logger.info(f"Returning key with content-type: {content_type}")

            # Return with the correct content type for PGP keys
            return Response(
                content=key_data,
                media_type=content_type,
                headers={
                    "Content-Disposition": 'attachment; filename="pubkey.asc"',
                    "X-HKP-Status": "Success",
                    "X-HKP-Key-Type": "public",
                },
            )
        else:
            logger.warning(f"Key not found for search: {search}")
            raise HTTPException(status_code=404, detail=f"Key not found for {search}")

    elif op == "index":
        # List or search through keys
        keys = list_all_keys()

        # Filter by search term if provided
        if search:
            filtered_keys = []
            for key in keys:
                match_found = False

                # Email matching
                if exact_search:
                    # Exact match against email or UIDs
                    if key["email"].lower() == search.lower():
                        match_found = True
                    else:
                        for uid in key["uids"]:
                            if uid.lower() == search.lower():
                                match_found = True
                                break
                else:
                    # Substring match against email or UIDs
                    if search.lower() in key["email"].lower():
                        match_found = True
                    else:
                        for uid in key["uids"]:
                            if search.lower() in uid.lower():
                                match_found = True
                                break

                # Key ID or fingerprint matching
                if not match_found and (
                    search.startswith("0x") or re.match(r"^[A-Fa-f0-9]{8,40}$", search)
                ):
                    search_term = search.upper().replace("0X", "")
                    if (
                        search_term in key["fingerprint"].upper()
                        or search_term in key["keyid"].upper()
                    ):
                        match_found = True

                if match_found:
                    filtered_keys.append(key)

            keys = filtered_keys

        # Format the response according to the HKP specification
        response = f"info:1:{len(keys)}\n"

        show_fingerprint = fingerprint == "on"

        for key in keys:
            # Format the pub line according to spec
            # pub:<keyid>:<algo>:<keylen>:<creationdate>:<expirationdate>:<flags>
            key_id = key["fingerprint"] if show_fingerprint else key["keyid"]
            response += f"pub:{key_id}:{key['algo']}:{key['keylen']}:{key['created']}:{key['expires']}:{key['flags']}\n"

            # Add all UIDs associated with the key
            # uid:<escaped uid string>:<creationdate>:<expirationdate>:<flags>
            for uid in key["uids"]:
                # Escape the UID string per the specification
                escaped_uid = urllib.parse.quote(uid, safe="@(),.=&-_~!")
                response += f"uid:{escaped_uid}:{key['created']}:{key['expires']}:{key['flags']}\n"

        return response

    else:
        raise HTTPException(status_code=400, detail="Invalid operation")


@app.get("/keys/{email}")
def get_key(email: str):
    """
    Get a specific key by email address.
    """
    email = sanitize_email(email)
    key_data = get_key_by_email(email)
    if key_data:
        return Response(content=key_data, media_type="application/pgp-keys")
    else:
        raise HTTPException(status_code=404, detail=f"Key not found for {email}")


@app.get("/keys")
def list_keys():
    """
    List all available keys.
    """
    keys = list_all_keys()
    return {
        "keys": [
            {
                "email": key["email"],
                "url": f"/keys/{key['email']}",
                "keyid": key["keyid"],
                "fingerprint": key["fingerprint"],
                "created": key["created"],
                "expires": key["expires"],
                "algo": key["algo"],
                "keylen": key["keylen"],
                "flags": key["flags"],
                "uids": key["uids"],
            }
            for key in keys
        ]
    }


@app.get("/pks/lookup/{keyid}")
def lookup_by_id(
    keyid: str = PathParam(
        ...,
        pattern="^[A-Fa-f0-9]{8,40}$",
        min_length=8,
        max_length=40,
        description="8–40 hex digits",
    ),
):
    """
    Direct key lookup by ID.
    Some GPG clients will attempt to retrieve keys using this pattern rather than query parameters.
    """
    logger.info(f"Direct key lookup by ID: {keyid}")

    # Search for the key by ID
    for key_file in Path(KEYS_DIR).glob("*.pgp"):
        key_info = extract_key_info(key_file)
        if key_info and (
            keyid.upper() in key_info["fingerprint"].upper()
            or keyid.upper() in key_info["keyid"].upper()
        ):
            with open(key_file, "r") as f:
                key_data = f.read()

                # Return with the correct content type
                return Response(
                    content=key_data,
                    media_type="application/pgp-keys",
                    headers={
                        "Content-Disposition": f'attachment; filename="{keyid}.asc"',
                        "X-HKP-Status": "Success",
                        "X-HKP-Key-Type": "public",
                    },
                )

    # If we get here, no key was found
    raise HTTPException(status_code=404, detail=f"Key not found: {keyid}")


if __name__ == "__main__":
    logger.info(f"Starting key-server on {HOST}:{PORT}")
    logger.info(f"Serving keys from {KEYS_DIR}")
    uvicorn.run(app, host=HOST, port=PORT)
