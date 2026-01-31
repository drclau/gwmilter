import email
from email import message_from_file, policy
from email.message import EmailMessage, Message
from pathlib import Path


def get_eml_files(eml_dir: Path | str) -> list[Path]:
    """Get all .eml files from the directory."""
    return list(Path(eml_dir).glob("*.eml"))


def create_email_from_file(
    eml_file: Path | str, from_addr: str, to_addrs: list[str], subject: str
) -> tuple[EmailMessage, str, str]:
    """Create an email message from a file and set headers.

    Returns:
        Tuple containing:
        - The email message object
        - The message ID
        - The original email body content as a string
    """
    # Parse the email file
    with open(eml_file, "r", encoding="utf-8") as f:
        email_message = message_from_file(f, policy=policy.SMTP)

    # Get the body before the headers are set, to facilitate byte-for-byte comparison
    original_body = email_message.as_string()

    # Set the headers explicitly
    message_id = email.utils.make_msgid()
    del email_message["Message-ID"]
    email_message["Message-ID"] = message_id
    email_message["From"] = from_addr
    email_message["To"] = ", ".join(to_addrs)
    email_message["Subject"] = subject

    # Re-parse the message to ensure line-endings are normalized
    email_message_str = email_message.as_string()
    email_message = email.message_from_string(email_message_str, policy=policy.SMTP)

    return email_message, message_id, original_body


def is_pgp_encrypted(email_message: Message | str) -> bool:
    """
    Check if an email message contains PGP encrypted content according to RFC3156.

    Args:
        email_message: An email.message.Message object or a string containing the email content

    Returns:
        bool: True if the email contains PGP encrypted content, False otherwise
    """
    if isinstance(email_message, str):
        email_message = email.message_from_string(email_message)

    if email_message.get_content_type() != "multipart/encrypted":
        return False

    if email_message.get_param("protocol") != "application/pgp-encrypted":
        return False

    parts = list(email_message.walk())
    if len(parts) != 3:
        return False

    first_part = parts[1]
    if first_part.get_content_type() != "application/pgp-encrypted":
        return False
    if "Version: 1" not in first_part.get_payload():
        return False

    return parts[2].get_content_type() == "application/octet-stream"


def _decode_payload(payload: bytes | str, charset: str) -> str:
    """Decode a payload from bytes to str, falling back to lossy decoding on error."""
    if isinstance(payload, str):
        return payload
    try:
        return payload.decode(charset)
    except UnicodeDecodeError:
        return payload.decode(charset, errors="replace")


def compare_parts_unordered(
    parts1: list[Message],
    parts2: list[Message],
    compare_headers: list[str] | None = None,
    ignore_attachments: bool = False,
) -> bool:
    """
    Compare two lists of email parts regardless of their order.

    Args:
        parts1: List of email parts from first message
        parts2: List of email parts from second message
        compare_headers: Optional list of header names to compare
        ignore_attachments: If True, skip comparison of attachment parts

    Returns:
        bool: True if the parts match (regardless of order), False otherwise
    """
    if len(parts1) != len(parts2):
        return False

    # Create copies of the lists to avoid modifying the originals
    remaining_parts2 = list(parts2)

    for part1 in parts1:
        # Try to find a matching part in the second list
        match_found = False

        for i, part2 in enumerate(remaining_parts2):
            if compare_email_content(part1, part2, compare_headers, ignore_attachments):
                # Found a matching part - remove it from the remaining parts
                remaining_parts2.pop(i)
                match_found = True
                break

        if not match_found:
            # No matching part found for this part1
            return False

    # If we've matched all parts, the lists are equivalent
    return True


def compare_email_content(
    msg1: Message,
    msg2: Message,
    compare_headers: list[str] | None = None,
    ignore_attachments: bool = False,
) -> bool:
    """
    Compare two email message objects based on their content and optionally headers.
    For multipart messages, recursively compare all parts.

    Args:
        msg1: First email message object
        msg2: Second email message object
        compare_headers: Optional list of header names to compare
        ignore_attachments: If True, skip comparison of attachment parts

    Returns:
        bool: True if content (and specified headers) are equal, False otherwise
    """
    # Compare specified headers if requested
    if compare_headers:
        for header in compare_headers:
            if msg1.get(header) != msg2.get(header):
                return False

    # Check if both messages have the same MIME type
    if msg1.get_content_type() != msg2.get_content_type():
        return False

    # Check if both are multipart
    is_multipart1 = msg1.is_multipart()
    is_multipart2 = msg2.is_multipart()

    if is_multipart1 != is_multipart2:
        return False

    if not is_multipart1:
        # For non-multipart, compare the payload

        # Get content maintype to handle binary content appropriately
        maintype = msg1.get_content_maintype()

        # Use get_payload(decode=True) to handle encoded content
        payload1 = msg1.get_payload(decode=True)
        payload2 = msg2.get_payload(decode=True)

        # Handle cases where payload might be None (e.g., empty content)
        if payload1 is None and payload2 is None:
            return True
        if payload1 is None or payload2 is None:
            return False

        # For binary content (applications, images, etc.), compare raw bytes
        if maintype in ("application", "image", "audio", "video"):
            return payload1 == payload2

        # For text content, decode and compare
        charset1 = msg1.get_content_charset() or "utf-8"
        charset2 = msg2.get_content_charset() or "utf-8"

        return _decode_payload(payload1, charset1) == _decode_payload(
            payload2, charset2
        )

    # For multipart messages, compare all parts recursively
    parts1 = msg1.get_payload()
    parts2 = msg2.get_payload()

    # Check if number of parts is the same
    if len(parts1) != len(parts2):
        return False

    # Handle attachment skipping if requested
    if ignore_attachments:
        # Filter out parts that are attachments
        filtered_parts1 = [p for p in parts1 if not p.get_filename()]
        filtered_parts2 = [p for p in parts2 if not p.get_filename()]

        # If the filtered parts count doesn't match, emails are different
        if len(filtered_parts1) != len(filtered_parts2):
            return False

        # Use order-independent matching for non-attachment parts
        return compare_parts_unordered(
            filtered_parts1, filtered_parts2, compare_headers, ignore_attachments
        )

    # Compare all parts, including attachments, regardless of order
    return compare_parts_unordered(parts1, parts2, compare_headers, ignore_attachments)
