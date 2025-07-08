from __future__ import annotations

from account_by_key_api.account_by_key_api_description import account_by_key_api_description
from api_generation.generate_client import generate_client


if __name__ == "__main__":
    generate_client(
        "account_by_key_api",
        account_by_key_api_description,
    )
