import pytest


@pytest.mark.testnet
def test_set_voting_proxy(wallet):
    wallet.api.create_account('initminer', 'alice', "{}")
    wallet.api.set_voting_proxy('initminer', 'alice')
