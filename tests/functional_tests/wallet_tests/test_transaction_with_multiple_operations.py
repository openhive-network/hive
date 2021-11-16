import pytest

from test_tools import Asset, Wallet


@pytest.fixture
def wallet(world):
    node = world.create_init_node()
    node.run()

    return Wallet(attach_to=node)


def test_sending_transaction_with_multiple_operations(wallet):
    accounts_and_balances = {
        'first': Asset.Test(100),
        'second': Asset.Test(200),
        'third': Asset.Test(300),
    }

    with wallet.in_single_transaction():
        for account, amount in accounts_and_balances.items():
            wallet.api.create_account('initminer', account, '{}')
            wallet.api.transfer('initminer', account, amount, 'memo')

    for account, expected_balance in accounts_and_balances.items():
        balance = wallet.api.get_account(account)['balance']
        assert balance == expected_balance


def test_sending_transaction_with_multiple_operations_without_broadcast(wallet):
    with wallet.in_single_transaction(broadcast=False) as transaction:
        wallet.api.create_account('initminer', 'alice', '{}')

    # Generated transaction can be accessed
    assert transaction.get_response() is not None

    # Transaction isn't send
    response = wallet.api.list_accounts('', 100)
    assert 'alice' not in response


def test_setting_broadcast_when_building_transaction(wallet):
    """During transaction building every wallet api call shouldn't be broadcasted.

    This test checks if when user do this, appropriate error is generated."""

    with wallet.in_single_transaction():
        with pytest.raises(RuntimeError):
            wallet.api.create_account('initminer', 'alice', '{}', True)


def test_getting_response(wallet):
    with wallet.in_single_transaction() as transaction:
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.transfer('initminer', 'alice', Asset.Test(100), 'memo')

    assert transaction.get_response() is not None
