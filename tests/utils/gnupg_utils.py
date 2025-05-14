import os
import tempfile
import shutil
from typing import List, Optional
import gnupg


class GPGTestWrapper:
    """A wrapper for python-gnupg that manages temporary GPG key storage and operations.

    This class creates a temporary directory for GPG keys, provides methods to import
    keys, and offers functionality to encrypt/decrypt content for testing purposes.
    """

    def __init__(self):
        """Initialize the GPG wrapper with a temporary directory for key storage."""
        self.temp_dir = tempfile.mkdtemp(prefix="gpg_test_")
        self.gpg = gnupg.GPG(gnupghome=self.temp_dir)
        self.gpg.encoding = "utf-8"

    def __enter__(self):
        """Context manager entry."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit - clean up temporary directory."""
        self.cleanup()

    def cleanup(self):
        """Remove the temporary directory and all its contents."""
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)

    def import_keys(self, key_paths: List[str]) -> List[str]:
        """Import GPG keys from the provided file paths.

        Args:
            key_paths: List of paths to GPG key files

        Returns:
            List of imported key fingerprints
        """
        imported_keys = []
        for key_path in key_paths:
            with open(key_path, "r") as key_file:
                import_result = self.gpg.import_keys(key_file.read())
                if import_result.count == 1:
                    imported_keys.append(import_result.fingerprints[0])
                else:
                    raise ValueError(f"Failed to import key from {key_path}")
        return imported_keys

    def encrypt(self, data: str, recipients: List[str]) -> str:
        """Encrypt data for the specified recipients.

        Args:
            data: String to encrypt
            recipients: List of recipient key fingerprints

        Returns:
            Encrypted data as string
        """
        encrypted = self.gpg.encrypt(data, recipients, armor=True)
        if not encrypted.ok:
            raise ValueError(f"Encryption failed: {encrypted.status}")
        return str(encrypted)

    def decrypt(self, encrypted_data: str, passphrase: Optional[str] = None) -> str:
        """Decrypt data using the imported private key.

        Args:
            encrypted_data: Encrypted data as string
            passphrase: Optional passphrase for the private key

        Returns:
            Decrypted data as string
        """
        decrypted = self.gpg.decrypt(encrypted_data, passphrase=passphrase)
        if not decrypted.ok:
            raise ValueError(f"Decryption failed: {decrypted.status}")
        return str(decrypted)

    def verify(self, data: str, signature: str) -> bool:
        """Verify a signature against the data.

        Args:
            data: Original data as string
            signature: GPG signature as string

        Returns:
            True if signature is valid, False otherwise
        """
        verified = self.gpg.verify(signature, data)
        return verified.valid

    def sign(
        self, data: str, key_fingerprint: str, passphrase: Optional[str] = None
    ) -> str:
        """Sign data with the specified key.

        Args:
            data: Data to sign
            key_fingerprint: Fingerprint of the signing key
            passphrase: Optional passphrase for the private key

        Returns:
            Signature as string
        """
        signed = self.gpg.sign(
            data, keyid=key_fingerprint, passphrase=passphrase, armor=True
        )
        if not signed.ok:
            raise ValueError(f"Signing failed: {signed.status}")
        return str(signed)
