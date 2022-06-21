import pytest


@pytest.mark.testnet
def test_create_account(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
