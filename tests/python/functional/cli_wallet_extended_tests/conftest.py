from __future__ import annotations

import pytest

import test_tools as tt


@pytest.fixture()
def wallet(node) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


@pytest.fixture()
def old_wallet(node) -> tt.OldWallet:
    return tt.OldWallet(attach_to=node)
