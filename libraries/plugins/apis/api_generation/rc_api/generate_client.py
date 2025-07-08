from __future__ import annotations

from rc_api.rc_api_description import rc_api_description
from api_generation.generate_client import generate_client


if __name__ == "__main__":
    generate_client(
        "rc_api",
        rc_api_description,
    )
