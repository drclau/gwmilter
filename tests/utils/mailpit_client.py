"""
Mailpit API Client and Testing Utilities

This module provides a client for interacting with the Mailpit SMTP testing server API,
along with utilities for testing email delivery in E2E tests.
Note: the client does not implement the full Mailpit API.

The module consists of:
1. Data models for Mailpit API responses (Message, MessageSummary, etc.).
2. MailpitClient class for interacting with the Mailpit API.
3. Utility functions for waiting and verifying email delivery.

Typical usage:
    client = MailpitClient("http://localhost:8025")
    messages = client.get_messages()
    specific_message = client.get_message(message_id)

For testing:
    received_ids = client.wait_for_messages(
        message_id, ["recipient@example.com"]
    )
"""

import time
from dataclasses import dataclass, field
from typing import Dict, List, NewType, Optional

import requests
from dataclasses_json import LetterCase, config, dataclass_json

# Custom types
ID = NewType("ID", str)


# Data Models


@dataclass_json(letter_case=LetterCase.PASCAL)
@dataclass
class Recipient:
    address: str
    name: Optional[str] = None


@dataclass_json(letter_case=LetterCase.CAMEL)
@dataclass
class MessageAttachment:
    content_id: Optional[str] = None
    content_type: Optional[str] = None
    file_name: Optional[str] = None
    part_id: Optional[str] = None
    size: Optional[int] = None


@dataclass_json(letter_case=LetterCase.PASCAL)
@dataclass
class Message:
    id: ID = field(metadata=config(field_name="ID"))
    from_: Recipient = field(metadata=config(field_name="From"))
    subject: str = field(metadata=config(field_name="Subject"))
    size: float = field(metadata=config(field_name="Size"))
    message_id: Optional[str] = field(
        default=None, metadata=config(field_name="MessageID")
    )
    date: Optional[str] = field(default=None, metadata=config(field_name="Date"))
    to: List[Recipient] = field(default_factory=list, metadata=config(field_name="To"))
    cc: Optional[List[Recipient]] = field(
        default=None, metadata=config(field_name="Cc")
    )
    bcc: Optional[List[Recipient]] = field(
        default=None, metadata=config(field_name="Bcc")
    )
    reply_to: List[Recipient] = field(
        default_factory=list, metadata=config(field_name="ReplyTo")
    )
    return_path: Optional[str] = field(
        default=None, metadata=config(field_name="ReturnPath")
    )
    text: Optional[str] = field(default=None, metadata=config(field_name="Text"))
    html: Optional[str] = field(default=None, metadata=config(field_name="HTML"))
    tags: List[str] = field(default_factory=list, metadata=config(field_name="Tags"))
    attachments: List[MessageAttachment] = field(
        default_factory=list, metadata=config(field_name="Attachments")
    )
    inline: List[MessageAttachment] = field(
        default_factory=list, metadata=config(field_name="Inline")
    )


@dataclass_json(letter_case=LetterCase.PASCAL)
@dataclass
class MessageSummary:
    id: ID = field(metadata=config(field_name="ID"))
    from_: Recipient = field(metadata=config(field_name="From"))
    subject: str = field(metadata=config(field_name="Subject"))
    size: float = field(metadata=config(field_name="Size"))
    message_id: Optional[str] = field(
        default=None, metadata=config(field_name="MessageID")
    )
    created: Optional[str] = field(default=None, metadata=config(field_name="Created"))
    to: List[Recipient] = field(default_factory=list, metadata=config(field_name="To"))
    cc: Optional[List[Recipient]] = field(
        default=None, metadata=config(field_name="Cc")
    )
    bcc: Optional[List[Recipient]] = field(
        default=None, metadata=config(field_name="Bcc")
    )
    reply_to: List[Recipient] = field(
        default_factory=list, metadata=config(field_name="ReplyTo")
    )
    snippet: Optional[str] = field(default=None, metadata=config(field_name="Snippet"))
    read: bool = field(default=False, metadata=config(field_name="Read"))
    attachments: int = field(default=0, metadata=config(field_name="Attachments"))
    tags: List[str] = field(default_factory=list, metadata=config(field_name="Tags"))


@dataclass_json(letter_case=LetterCase.SNAKE)
@dataclass
class MessagesSummary:
    count: int
    messagesCount: int
    unread: int
    messages: List[MessageSummary] = field(default_factory=list)
    tags: List[str] = field(default_factory=list)
    start: int = 0
    totalTags: int = 0
    total: int = 0


@dataclass_json(letter_case=LetterCase.CAMEL)
@dataclass
class MessageHeadersResponse:
    headers: List[Dict[str, str]] = field(default_factory=list)


# Client Implementation
class MailpitClient:
    def __init__(self, base_url: str = "http://localhost:8025"):
        self.base_url = base_url.rstrip("/")
        self.api_url = f"{self.base_url}/api/v1"

    def get_messages(
        self,
        start: int = 0,
        limit: int = 50,
        search: Optional[str] = None,
        tags: Optional[List[str]] = None,
        unread: Optional[bool] = None,
    ) -> MessagesSummary:
        """Get messages summary"""
        params = {"start": start, "limit": limit}
        if search:
            params["search"] = search
        if tags:
            params["tags"] = ",".join(tags)
        if unread is not None:
            params["unread"] = "1" if unread else "0"

        try:
            response = requests.get(f"{self.api_url}/messages", params=params)
            response.raise_for_status()
            data = response.json()
            return MessagesSummary.from_dict(data)
        except requests.exceptions.RequestException as e:
            print(f"Error fetching messages: {e}")
            raise

    def get_message(self, message_id: ID) -> Message:
        """Get a specific message"""
        response = requests.get(f"{self.api_url}/message/{message_id}")
        response.raise_for_status()
        data = response.json()
        # Handle 'from' field as it's a Python reserved keyword
        if "from" in data:
            data["from_"] = data.pop("from")
        return Message.from_dict(data)

    def get_message_headers(self, message_id: ID) -> MessageHeadersResponse:
        """Get message headers"""
        response = requests.get(f"{self.api_url}/message/{message_id}/headers")
        response.raise_for_status()
        data = response.json()
        return MessageHeadersResponse.from_dict(data)

    def get_message_source(self, message_id: ID) -> str:
        """Get the raw source of a message"""
        response = requests.get(f"{self.api_url}/message/{message_id}/raw")
        response.raise_for_status()
        return response.text

    def get_attachment(self, message_id: ID, part_id: str) -> bytes:
        """Get a message attachment"""
        response = requests.get(f"{self.api_url}/message/{message_id}/part/{part_id}")
        response.raise_for_status()
        return response.content

    def get_attachment_thumbnail(self, message_id: ID, part_id: str) -> bytes:
        """Get an attachment thumbnail image"""
        response = requests.get(
            f"{self.api_url}/message/{message_id}/part/{part_id}/thumb"
        )
        response.raise_for_status()
        return response.content

    def delete_message(self, message_id: ID) -> None:
        """Delete a message"""
        response = requests.delete(f"{self.api_url}/message/{message_id}")
        response.raise_for_status()

    def delete_all_messages(self) -> None:
        """Delete all messages"""
        response = requests.delete(f"{self.api_url}/messages")
        response.raise_for_status()

    def mark_message_read(self, message_id: ID) -> None:
        """Mark a message as read"""
        response = requests.put(f"{self.api_url}/message/{message_id}/read")
        response.raise_for_status()

    def mark_message_unread(self, message_id: ID) -> None:
        """Mark a message as unread"""
        response = requests.put(f"{self.api_url}/message/{message_id}/unread")
        response.raise_for_status()

    def get_tags(self) -> List[str]:
        """Get all tags"""
        response = requests.get(f"{self.api_url}/tags")
        response.raise_for_status()
        return response.json()

    def tag_message(self, message_id: ID, tags: List[str]) -> None:
        """Tag a message"""
        response = requests.put(f"{self.api_url}/message/{message_id}/tags", json=tags)
        response.raise_for_status()

    def wait_for_messages(
        self,
        message_id,
        to_addrs,
        count=1,
        max_wait_time=5,
        check_interval=0.5,
        extra_cycles=3,
    ) -> List[ID]:
        """
        Wait for a specified number of emails to arrive in mailpit and return their message IDs.

        Args:
            message_id: The expected message ID to look for
            to_addrs: List of recipient addresses to check
            count: Number of messages to wait for (default: 1)
            max_wait_time: Maximum time to wait in seconds
            check_interval: Time between checks in seconds
            extra_cycles: Number of additional cycles to wait after finding expected count

        Returns:
            List of received message IDs (as ID type)
        """
        if isinstance(to_addrs, str):
            to_addrs = [to_addrs]

        end_time = time.time() + max_wait_time

        found_messages: List[ID] = []
        cycles_after_found = 0
        required_count_found = False

        while time.time() < end_time and (
            len(found_messages) < count or cycles_after_found < extra_cycles
        ):
            # Get messages from mailpit
            messages = self.get_messages(limit=50)

            # Check if any message matches our recipient and has the expected message ID
            for msg in messages.messages:
                # Skip if we've already found this message
                if msg.id in found_messages:
                    continue

                # Check if the message ID matches
                if f"<{msg.message_id}>" == message_id:
                    # Also verify the recipient matches
                    recipients = [to.address for to in msg.to]
                    if any(recipient in recipients for recipient in to_addrs):
                        found_messages.append(ID(msg.id))
                        print(
                            f"Email found in mailpit with message ID: {msg.message_id}"
                        )
                        if len(found_messages) >= count and not required_count_found:
                            required_count_found = True

            # If we've found the required count, start counting extra cycles
            if required_count_found:
                cycles_after_found += 1
                print(
                    f"Found required count, waiting extra cycle {cycles_after_found}/{extra_cycles}"
                )

            time.sleep(check_interval)

        return found_messages
