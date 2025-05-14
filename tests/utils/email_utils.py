import email
from email import message_from_file, policy
from email.message import EmailMessage
from email.mime.multipart import MIMEMultipart
from pathlib import Path
from typing import List, Optional, Tuple, Union

from tests.utils.mailpit_client import Message


def get_eml_files(eml_dir) -> List[str]:
    """Get all .eml files from the directory."""
    p = Path(eml_dir)
    return [str(f) for f in p.glob("*.eml")]


def create_email_from_file(
    eml_file, from_addr, to_addrs, subject
) -> Tuple[EmailMessage, str, str]:
    """Create an email message from a file and set headers.

    Returns:
        Tuple containing:
        - The email message object
        - The message ID
        - The original email body content as a string
    """
    # Parse the email file
    with open(eml_file, "r") as f:
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


def is_pgp_encrypted(email_message: Union[Message, str]) -> bool:
    """
    Check if an email message contains PGP encrypted content according to RFC3156.

    Args:
        email_message: An email.message.Message object or a string containing the email content

    Returns:
        bool: True if the email contains PGP encrypted content, False otherwise
    """
    if isinstance(email_message, str):
        email_message = email.message_from_string(email_message)

    # Check for PGP/MIME content type according to RFC3156
    content_type = email_message.get_content_type()
    if content_type == "multipart/encrypted":
        protocol = email_message.get_param("protocol")
        if protocol == "application/pgp-encrypted":
            # Get all parts using walk()
            parts = list(email_message.walk())
            if len(parts) == 3:
                # First part must be application/pgp-encrypted with Version: 1
                first_part = parts[1]
                if (
                    first_part.get_content_type() == "application/pgp-encrypted"
                    and "Version: 1" in first_part.get_payload()
                ):
                    # Second part must be application/octet-stream
                    second_part = parts[2]
                    if second_part.get_content_type() == "application/octet-stream":
                        return True

    return False


def compare_parts_unordered(
    parts1: List[Union[EmailMessage, MIMEMultipart]],
    parts2: List[Union[EmailMessage, MIMEMultipart]],
    compare_headers: Optional[List[str]] = None,
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
    msg1: Union[EmailMessage, MIMEMultipart],
    msg2: Union[EmailMessage, MIMEMultipart],
    compare_headers: Optional[List[str]] = None,
    ignore_attachments: bool = False,
) -> bool:
    """
    Compare two email message objects based on their content and optionally headers.
    For multipart messages, recursively compare all parts.

    Args:
        msg1: First email message object (EmailMessage or MIMEMultipart)
        msg2: Second email message object (EmailMessage or MIMEMultipart)
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

        try:
            # Try strict decoding first
            if isinstance(payload1, bytes):
                payload1 = payload1.decode(charset1)
            if isinstance(payload2, bytes):
                payload2 = payload2.decode(charset2)
        except UnicodeDecodeError:
            # Fall back to replacement if strict decoding fails
            if isinstance(payload1, bytes):
                payload1 = payload1.decode(charset1, errors="replace")
            if isinstance(payload2, bytes):
                payload2 = payload2.decode(charset2, errors="replace")

        return payload1 == payload2

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
