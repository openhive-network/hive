import pytest


@pytest.mark.testnet
def test_sign_transaction(wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    wallet.api.sign_transaction(transaction, broadcast=False)
