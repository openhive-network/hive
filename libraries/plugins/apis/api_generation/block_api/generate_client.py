from __future__ import annotations

from block_api.block_api_description import block_api_description
from api_generation.generate_client import generate_client


if __name__ == "__main__":
    generate_client(
        "block_api",
        block_api_description,
    )
