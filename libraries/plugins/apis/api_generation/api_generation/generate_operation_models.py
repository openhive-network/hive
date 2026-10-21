# ruff: noqa: INP001
"""Generate models of operations and transactions (hiveio_api.operations, hiveio_api._validation.operations)."""

from __future__ import annotations

import sys
from pathlib import Path

from api_generation.generate_operations import generate_operations

if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise ValueError(
            "Usage: python generate_operation_models.py <operations_description.json> <openapi.json> <base_directory>"
        )

    for path in generate_operations(Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])):
        print(f"Generated {path}")
