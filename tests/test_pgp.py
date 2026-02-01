import email
import smtplib
from email import message_from_string, policy
from typing import NamedTuple

import pytest

from tests.utils.email_utils import (
    compare_email_content,
    create_email_from_file,
    get_eml_files,
    is_pgp_encrypted,
)
from tests.utils.gnupg_utils import GPGTestWrapper
from tests.utils.mailpit_client import MailpitClient
from tests.utils.path_utils import PROJECT_ROOT

eml_dir = PROJECT_ROOT / "tests" / "eml"
eml_files = get_eml_files(eml_dir)

# Define the key filenames to be used in tests
PGP_KEY_PATHS = [
    "private/pgp-valid-present-01@example.com.pgp",
    "private/pgp-valid-present-02@example.com.pgp",
    "private/pgp-expired-present-01@example.com.pgp",
    "private/pgp-valid-missing-01@example.com.pgp",
]

# Define test scenarios for single protocol handler
single_protocol_scenarios = [
    pytest.param(
        {"to_addrs": ["pgp-valid-present-01@example.com"], "expected_count": 1},
        id="single_recipient",
    ),
    pytest.param(
        {
            "to_addrs": [
                "pgp-valid-present-01@example.com",
                "pgp-valid-present-02@example.com",
            ],
            "expected_count": 1,
        },
        id="multiple_recipients",
    ),
]

# Define test scenarios for multiple protocol handlers
multiple_protocol_scenarios = [
    pytest.param(
        {
            "to_addrs": ["pgp-valid-present-01@example.com", "no-key@example.com"],
            "expected_count": 2,
        },
        id="pgp_and_none",
    ),
    pytest.param(
        {
            "to_addrs": [
                "pgp-valid-present-01@example.com",
                "pgp-valid-present-02@example.com",
                "no-key@example.com",
            ],
            "expected_count": 2,
        },
        id="multiple_pgp_and_none",
    ),
]

# Define test scenarios for key retrieval from keyserver
keyserver_scenarios = [
    pytest.param(
        {"to_addrs": ["pgp-valid-missing-01@example.com"], "expected_count": 1},
        id="keyserver_recipient",
    ),
]

# Define test scenario for expired key
expired_key_scenarios = [
    pytest.param(
        {"to_addrs": ["pgp-expired-present-01@example.com"], "expected_count": 0},
        id="expired_pgp_recipient",
    ),
]


class EmailTestSetup(NamedTuple):
    from_addr: str
    to_addrs: list[str]
    expected_email_count: int
    smtp_client: smtplib.SMTP
    mailpit_client: MailpitClient
    gpg_wrapper: GPGTestWrapper


@pytest.fixture
def email_test_setup(request, smtp_client, mailpit_client, gpg_wrapper, keys_dir):
    """Fixture that sets up the email test based on the scenario parameter."""
    # Get parameters from the request
    params = request.param
    to_addrs = params["to_addrs"]
    expected_count = params["expected_count"]
    from_addr = "sender@example.com"

    # Import private keys using configurable keys_dir and PGP_KEY_PATHS
    gpg_wrapper.import_keys([f"{keys_dir}/{key_path}" for key_path in PGP_KEY_PATHS])

    return EmailTestSetup(
        from_addr=from_addr,
        to_addrs=to_addrs,
        expected_email_count=expected_count,
        smtp_client=smtp_client,
        mailpit_client=mailpit_client,
        gpg_wrapper=gpg_wrapper,
    )


def assert_pgp_encrypted_delivery(
    setup: EmailTestSetup,
    message_str: str,
    message_id: str,
    original_body: str,
    extra_cycles: int = 1,
) -> None:
    """Send an email and verify it arrives PGP-encrypted with correct content."""
    result = setup.smtp_client.sendmail(
        setup.from_addr, setup.to_addrs, message_str
    )
    assert not result, f"Failed to send email to some recipients: {result}"

    received_message_ids = setup.mailpit_client.wait_for_messages(
        message_id, setup.to_addrs, extra_cycles=extra_cycles
    )
    assert len(received_message_ids) == setup.expected_email_count, (
        f"Expected to find {setup.expected_email_count} emails for "
        f"recipient(s): {setup.to_addrs}, but found {len(received_message_ids)}"
    )

    received_id = received_message_ids[0]
    message_source = setup.mailpit_client.get_message_source(received_id)
    assert message_source, "Failed to get message source from mailpit"
    assert is_pgp_encrypted(message_source), "Message is not PGP encrypted"

    decrypted_content = setup.gpg_wrapper.decrypt(message_source)
    assert decrypted_content, "Failed to decrypt message content"
    assert decrypted_content == original_body, (
        "Decrypted content does not match original email body"
    )


@pytest.mark.parametrize("email_test_setup", single_protocol_scenarios, indirect=True)
@pytest.mark.parametrize("eml_file", eml_files, ids=[f.stem for f in eml_files])
def test_pgp_single_protocol_handler(eml_file, email_test_setup, request):
    """Test sending each email from the eml directory and verify PGP decryption."""
    email_message, message_id, original_body = create_email_from_file(
        eml_file, email_test_setup.from_addr, email_test_setup.to_addrs,
        request.node.name,
    )

    assert_pgp_encrypted_delivery(
        email_test_setup, email_message.as_string(), message_id, original_body,
    )


@pytest.mark.parametrize("email_test_setup", multiple_protocol_scenarios, indirect=True)
@pytest.mark.parametrize("eml_file", eml_files, ids=[f.stem for f in eml_files])
def test_pgp_multiple_protocol_handlers(eml_file, email_test_setup, request):
    """Test sending each email from the eml directory and verify PGP decryption."""
    setup = email_test_setup

    email_message, message_id, original_body = create_email_from_file(
        eml_file, setup.from_addr, setup.to_addrs, request.node.name,
    )

    result = setup.smtp_client.sendmail(
        setup.from_addr, setup.to_addrs, email_message.as_string()
    )
    assert not result, f"Failed to send email to some recipients: {result}"

    received_message_ids = setup.mailpit_client.wait_for_messages(
        message_id, setup.to_addrs, extra_cycles=3
    )
    assert len(received_message_ids) == setup.expected_email_count, (
        f"Expected to find {setup.expected_email_count} emails for "
        f"recipient(s): {setup.to_addrs}, but found {len(received_message_ids)}"
    )

    encrypted_count = 0
    unencrypted_count = 0

    for received_message_id in received_message_ids:
        message_source = setup.mailpit_client.get_message_source(received_message_id)
        assert message_source, "Failed to get message source from mailpit"

        if is_pgp_encrypted(message_source):
            encrypted_count += 1

            decrypted_content = setup.gpg_wrapper.decrypt(message_source)
            assert decrypted_content, "Failed to decrypt message content"
            assert decrypted_content == original_body, (
                "Decrypted content does not match original email body"
            )
        else:
            unencrypted_count += 1

            received_message = email.message_from_string(
                message_source, policy=policy.SMTP
            )
            original_message = email.message_from_string(
                email_message.as_string(), policy=policy.SMTP
            )

            assert compare_email_content(
                received_message,
                original_message,
                compare_headers=["Subject", "From"],
                ignore_attachments=False,
            ), "Unencrypted email content does not match original email"

    # Verify we have the right mix of encrypted and unencrypted emails
    if setup.expected_email_count == 2:
        assert encrypted_count == 1, (
            f"Expected 1 encrypted email, got {encrypted_count}"
        )
        assert unencrypted_count == 1, (
            f"Expected 1 unencrypted email, got {unencrypted_count}"
        )
    else:
        assert encrypted_count >= 1, "Expected at least 1 encrypted email"
        assert unencrypted_count >= 1, "Expected at least 1 unencrypted email"


@pytest.mark.parametrize("email_test_setup", keyserver_scenarios, indirect=True)
def test_pgp_recv_key_from_keyserver(
    email_test_setup, request, cleanup_keyserver_retrieved_keys
):
    """Test sending email to recipient with key available on keyserver."""
    setup = email_test_setup

    message_id = email.utils.make_msgid()
    content = (
        f"This is a test email {message_id} to verify key retrieval from keyserver.\n"
    )

    msg = message_from_string(content, policy=policy.SMTP)
    original_body = msg.as_string()

    msg["Message-ID"] = message_id
    msg["From"] = setup.from_addr
    msg["To"] = ", ".join(setup.to_addrs)
    msg["Subject"] = "Test Key Retrieval from Keyserver"

    assert_pgp_encrypted_delivery(
        setup, msg.as_string(), message_id, original_body,
    )


@pytest.mark.parametrize("email_test_setup", expired_key_scenarios, indirect=True)
@pytest.mark.parametrize(
    "eml_file", eml_files[:1], ids=[f.stem for f in eml_files[:1]]
)
def test_pgp_expired_key(eml_file, email_test_setup, request):
    """Test sending email to recipient with expired PGP key fails."""
    setup = email_test_setup

    email_message, message_id, original_body = create_email_from_file(
        eml_file, setup.from_addr, setup.to_addrs, request.node.name,
    )

    # The SMTP send should fail, so we use pytest.raises to catch the expected exception
    with pytest.raises(Exception) as excinfo:
        setup.smtp_client.sendmail(
            setup.from_addr, setup.to_addrs, email_message.as_string()
        )

    # Check that the SMTP error code is 550
    error_code = excinfo.value.args[0]
    assert error_code == 550, f"Expected SMTP error code 550, got: {error_code}"

    # Verify that no emails were delivered to mailpit
    # Use a small number of cycles to check that nothing arrives
    received_message_ids = setup.mailpit_client.wait_for_messages(
        message_id, setup.to_addrs, max_wait_time=2
    )

    # Verify that no emails were received
    assert len(received_message_ids) == 0, (
        f"Expected 0 emails to be delivered, but found {len(received_message_ids)}"
    )
