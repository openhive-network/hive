from __future__ import annotations

import pytest
import test_tools as tt


@pytest.mark.requires_hived_executables
@pytest.mark.parametrize(
    ("name", "expected_key"),
    [
        ("initminer", "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n"),
        ("alice", "5KTNAYSHVzhnVPrwHpKhc5QqNQt6aW8JsrMT7T4hyrKydzYvYik"),
    ],
)
def test_private_key_generation(name: str, expected_key: str) -> None:
    assert tt.Account(name).private_key == expected_key


@pytest.mark.requires_hived_executables
@pytest.mark.parametrize(
    ("name", "expected_key"),
    [
        ("initminer", "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"),
        ("alice", "STM5P8syqoj7itoDjbtDvCMCb5W3BNJtUjws9v7TDNZKqBLmp3pQW"),
    ],
)
def test_public_key_generation(name: str, expected_key: str) -> None:
    assert tt.Account(name).public_key == expected_key
