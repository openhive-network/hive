from __future__ import annotations

from typing import Callable

import pytest
import test_tools as tt

from schemas.fields.basic import PrivateKey, PublicKey

PrivateKeyFactory = Callable[[str], PrivateKey]
PublicKeyFactory = Callable[[str], PublicKey]


@pytest.mark.requires_hived_executables
def test_if_same_accounts_have_same_keys() -> None:
    assert tt.Account("alice") == tt.Account("alice")


@pytest.mark.requires_hived_executables
def test_if_different_accounts_have_different_keys() -> None:
    assert tt.Account("alice") != tt.Account("bob")


@pytest.mark.requires_hived_executables
@pytest.mark.parametrize("key", [tt.PrivateKey, tt.PublicKey])
def test_check_of_key_presence_in_set_of_keys(key: PrivateKeyFactory | PublicKeyFactory) -> None:
    assert key("bob") in {key(name) for name in ["alice", "bob", "carol"]}


@pytest.mark.requires_hived_executables
@pytest.mark.parametrize("key", [tt.PrivateKey, tt.PublicKey])
def test_check_of_key_presence_in_set_of_strings(key: PrivateKeyFactory | PublicKeyFactory) -> None:
    assert key("bob") in {str(key(name)) for name in ["alice", "bob", "carol"]}
