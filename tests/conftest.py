import os
import re
import subprocess
from typing import Any, Dict

import pytest

from tests.utils.gnupg_utils import GPGTestWrapper
from tests.utils.mailpit_client import MailpitClient
from tests.utils.path_utils import PROJECT_ROOT


def pytest_addoption(parser):
    parser.addini(
        "smtp_host", help="Default SMTP host to use for tests", default="localhost"
    )
    parser.addini("smtp_port", help="Default SMTP port to use for tests", default="25")
    parser.addoption(
        "--smtp-host", action="store", default=None, help="SMTP server hostname"
    )
    parser.addoption(
        "--smtp-port", action="store", default=None, help="SMTP server port"
    )

    parser.addini(
        "disable_email_cleanup",
        help="Disable email cleanup in mailpit_client fixture (bool, true/false)",
        default=None,
        type="bool",
    )
    parser.addoption(
        "--disable-email-cleanup",
        action="store_true",
        default=None,
        help="Disable email cleanup in mailpit_client fixture",
    )

    parser.addini(
        "keys_dir",
        help="Directory for PGP keys used in tests",
        default=None,
    )
    parser.addoption(
        "--keys-dir",
        action="store",
        default=None,
        help="Directory for PGP keys used in tests (overrides ini)",
    )

    parser.addini(
        "mailpit_url",
        help="Mailpit server base URL (e.g. http://localhost:8025)",
        default="http://localhost:8025",
    )
    parser.addoption(
        "--mailpit-url",
        action="store",
        default=None,
        help="Mailpit server base URL (overrides ini)",
    )

    parser.addini(
        "gwmilter_container",
        help="Name of gwmilter container. If not set, assumes local run.",
        default=None,
    )
    parser.addoption(
        "--gwmilter-container",
        action="store",
        default=None,
        help="Name of gwmilter container. If not set, assumes local run.",
    )

    parser.addoption(
        "--gwmilter-local",
        action="store_true",
        default=False,
        help="Run gwmilter locally. Overrides gwmilter_container (CLI or ini).",
    )


@pytest.fixture(scope="session")
def smtp_config(request: pytest.FixtureRequest):
    """SMTP configuration."""
    ini_host = request.config.getini("smtp_host")
    ini_port = request.config.getini("smtp_port")

    host = request.config.getoption("--smtp-host") or ini_host or "localhost"
    port_str = request.config.getoption("--smtp-port") or ini_port or "25"
    port = int(port_str)

    return {"host": host, "port": port}


@pytest.fixture
def smtp_client(smtp_config: Dict[str, Any]):
    """Create and yield an SMTP client connection."""
    import smtplib

    smtp = smtplib.SMTP(smtp_config["host"], smtp_config["port"])
    yield smtp
    smtp.close()


@pytest.fixture(scope="session")
def mailpit_url(request: pytest.FixtureRequest):
    """Mailpit server base URL from CLI or ini."""
    cli_value = request.config.getoption("--mailpit-url")
    ini_value = request.config.getini("mailpit_url")
    return cli_value or ini_value or "http://localhost:8025"


@pytest.fixture(scope="function")
def mailpit_client(request: pytest.FixtureRequest, mailpit_url):
    """Create and yield a MailpitClient instance using configured URL."""
    client = MailpitClient(base_url=mailpit_url)

    yield client

    if request.config.getoption("--disable-email-cleanup") or request.config.getini(
        "disable_email_cleanup"
    ):
        print("Email cleanup disabled")
        return

    # Clean up any remaining messages after tests
    try:
        # Only delete if all tests passed. This is useful for debugging.
        if request.session.testsfailed == 0:
            print("Deleting all messages from Mailpit")
            client.delete_all_messages()
        else:
            print("Not deleting messages from Mailpit because test failed")

    except Exception:
        pass  # Ignore cleanup errors


@pytest.fixture(scope="session")
def gpg_wrapper():
    """Create and yield a GPGTestWrapper instance.

    This fixture provides a GPG wrapper that manages temporary key storage
    and operations for testing. The wrapper is session-scoped to avoid
    recreating the GPG environment for each test.

    Example usage:
        def test_gpg_operations(gpg_wrapper):
            key_fingerprints = gpg_wrapper.import_keys(["path/to/key.asc"])
            encrypted = gpg_wrapper.encrypt("test data", key_fingerprints)
    """
    wrapper = GPGTestWrapper()
    yield wrapper
    wrapper.cleanup()


@pytest.fixture(scope="session", autouse=True)
def ensure_gnupghome():
    """Default GNUPGHOME for tests unless already set."""
    if "GNUPGHOME" not in os.environ or not os.environ["GNUPGHOME"]:
        os.environ["GNUPGHOME"] = str(PROJECT_ROOT / "integrations" / "gnupg")
        print(f"GNUPGHOME defaulted to: {os.environ['GNUPGHOME']}")


def run_key_cleanup(pattern, pgp_utils_args):
    """Run pgp-utils.sh delete-keys for the given pattern and fail if no keys are deleted."""
    cleanup_result = subprocess.run(
        [
            str(PROJECT_ROOT / "tests" / "scripts" / "pgp-utils.sh"),
            *pgp_utils_args,
            "delete-keys",
            "-f",
            pattern,
        ],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if cleanup_result.returncode == 0:
        # Parse the number of deleted keys from the output
        deleted_count = None
        for line in cleanup_result.stdout.splitlines():
            m = re.match(r"DELETED_KEYS_COUNT=(\d+)", line)
            if m:
                deleted_count = int(m.group(1))
                break
        if deleted_count is None:
            raise RuntimeError("could not determine number of deleted keys")
        elif deleted_count == 0:
            raise RuntimeError(f"keys following the pattern '{pattern}' were not found")
    else:
        raise RuntimeError(
            f"pgp-utils.sh delete-keys failed with code {cleanup_result.returncode}"
        )


def run_keyserver_reset(pgp_utils_args):
    """Run pgp-utils.sh reset-keyserver and raise on error."""
    reset_result = subprocess.run(
        [
            str(PROJECT_ROOT / "tests" / "scripts" / "pgp-utils.sh"),
            *pgp_utils_args,
            "reset-keyserver",
        ],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if reset_result.returncode != 0:
        raise RuntimeError(
            f"pgp-utils.sh reset-keyserver failed with code {reset_result.returncode}"
        )


@pytest.fixture
def cleanup_keyserver_retrieved_keys(request):
    """Fixture to clean up keys containing 'missing' after test."""
    pgp_utils_args = get_gwmilter_flag(request)
    try:
        print("Resetting keyserver to alive status in GPG dirmngr...")
        run_keyserver_reset(pgp_utils_args)
    except Exception as e:
        print(f"Error: failed to clean up keys: {e}")
        raise

    yield

    try:
        print("Cleaning up keys with 'missing' in them...")
        cleanup_pattern = "pgp-(valid|expired)-missing-[0-9]+@example.com"
        run_key_cleanup(cleanup_pattern, pgp_utils_args)
    except Exception as e:
        # Only raise if the test did not fail
        if request.session.testsfailed == 0:
            print(f"Error: failed to clean up keys: {e}")
            raise


@pytest.fixture(scope="session")
def keys_dir(request: pytest.FixtureRequest):
    """Fixture to get the keys directory from pytest config (CLI or ini)."""
    cli_value = request.config.getoption("--keys-dir")
    ini_value = request.config.getini("keys_dir")
    default_keys_dir = PROJECT_ROOT / "integrations" / "keys"
    print(f"Default keys directory: {default_keys_dir}")
    print(f"Keys directory: {cli_value or ini_value or str(default_keys_dir)}")
    return cli_value or ini_value or str(default_keys_dir)


def get_gwmilter_flag(request: pytest.FixtureRequest):
    # CLI --gwmilter-local overrides everything.
    if request.config.getoption("--gwmilter-local"):
        print("Running gwmilter locally")
        return ["--local"]

    container = request.config.getoption(
        "--gwmilter-container"
    ) or request.config.getini("gwmilter_container")
    if container:
        print(f"Running gwmilter in container: {container}")
        return ["--container", container]

    return ["--local"]
