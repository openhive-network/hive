"""Tests of the field types and validation of hiveio_api (formerly hiveio-schemas) - run on the generated package."""

from __future__ import annotations

from tests.generated_package import use_generated_package

if not use_generated_package():
    collect_ignore_glob = ["test_*.py"]
