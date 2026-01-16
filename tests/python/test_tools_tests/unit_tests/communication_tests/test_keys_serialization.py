from __future__ import annotations

import pytest
import test_tools as tt


@pytest.mark.requires_hived_executables
def test_private_key_serialization() -> None:
    alice = tt.Account("alice")
    assert str(alice.private_key) == "5KTNAYSHVzhnVPrwHpKhc5QqNQt6aW8JsrMT7T4hyrKydzYvYik"


@pytest.mark.requires_hived_executables
def test_public_key_serialization() -> None:
    alice = tt.Account("alice")
    assert str(alice.public_key) == "STM5P8syqoj7itoDjbtDvCMCb5W3BNJtUjws9v7TDNZKqBLmp3pQW"
