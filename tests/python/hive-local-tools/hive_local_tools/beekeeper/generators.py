from __future__ import annotations


def generate_wallet_name(number: int = 0) -> str:
    return f"wallet-{number}"


def generate_wallet_password(number: int = 0) -> str:
    return f"password-{number}"


def generate_account_name(number: int = 0) -> str:
    return f"account-{number}"


def default_wallet_credentials() -> tuple[str, str]:
    """Returns wallet name and password."""
    return (generate_wallet_name(), generate_wallet_password())
