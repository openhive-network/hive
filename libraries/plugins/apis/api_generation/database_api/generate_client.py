from __future__ import annotations

from database_api.database_api_description import database_api_description
from api_generation.generate_client import generate_client


if __name__ == "__main__":
    generate_client(
        "database_api",
        database_api_description,
    )
