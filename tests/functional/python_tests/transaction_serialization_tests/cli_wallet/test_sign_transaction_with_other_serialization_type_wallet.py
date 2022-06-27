import pytest

import test_tools as tt


@pytest.fixture
def wallet(request):
    node = tt.InitNode()
    node.run()
    return tt.Wallet(attach_to=node, additional_arguments=[f'--transaction-serialization={request.param}'])


@pytest.mark.parametrize(
    'wallet', [
        'legacy',
        'hf26',
    ], indirect=True
)
def test_sign_transaction_with_matching_wallet(wallet):
    transaction = wallet.api.create_account('initminer', 'alice', '{}', broadcast=False)
    wallet.api.sign_transaction(transaction)


def test_sign_legacy_transaction_with_nai_wallet(wallet_with_legacy_serialization, wallet_with_nai_serialization):
    legacy_transaction = wallet_with_legacy_serialization.api.create_account('initminer', 'alice', '{}', broadcast=False)
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet_with_nai_serialization.api.sign_transaction(legacy_transaction)


def test_sign_nai_transaction_with_legacy_wallet(wallet_with_legacy_serialization, wallet_with_nai_serialization):
    nai_transaction = wallet_with_nai_serialization.api.create_account('initminer', 'alice', '{}', broadcast=False)
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet_with_legacy_serialization.api.sign_transaction(nai_transaction)
