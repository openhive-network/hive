from __future__ import annotations


def generate_wallet_name(number: int = 0) -> str:
    return f"wallet-{number}"


def generate_wallet_password(number: int = 0) -> str:
    return f"password-{number}"
