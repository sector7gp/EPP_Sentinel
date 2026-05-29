from __future__ import annotations

import base64
import hashlib

from cryptography.fernet import Fernet

from app.config import get_settings


def _fernet_key(secret: str) -> bytes:
    digest = hashlib.sha256(secret.encode()).digest()
    return base64.urlsafe_b64encode(digest)


def encrypt_value(plain: str) -> str:
    f = Fernet(_fernet_key(get_settings().secret_key))
    return f.encrypt(plain.encode()).decode()


def decrypt_value(encrypted: str) -> str:
    f = Fernet(_fernet_key(get_settings().secret_key))
    return f.decrypt(encrypted.encode()).decode()


def mask_secret(value: str | None) -> str | None:
    if not value:
        return None
    return "••••" + value[-4:] if len(value) >= 4 else "••••"
