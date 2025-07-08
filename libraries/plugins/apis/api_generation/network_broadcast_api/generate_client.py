from __future__ import annotations

from network_broadcast_api.network_broadcast_api_description import network_broadcast_api_description
from api_generation.generate_client import generate_client


if __name__ == "__main__":
    generate_client(
        "network_broadcast_api",
        network_broadcast_api_description,
    )
