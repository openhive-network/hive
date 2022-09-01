import pytest


@pytest.mark.testnet
def test_uniqueness_of_transaction_ids_generated_by_wallet(node, wallet):
    already_generated_transaction_ids = set()

    for name in [f'account-{i:02d}' for i in range(100)]:
        transaction = wallet.api.create_account('initminer', name, '{}', broadcast=False)

        assert transaction['transaction_id'] not in already_generated_transaction_ids
        already_generated_transaction_ids.add(transaction['transaction_id'])
