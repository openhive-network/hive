import pytest

from test_tools import World


def test_sending_transaction_with_multiple_operations():
    with World() as world:
        node = world.create_init_node()
        node.config.plugin.append('network_broadcast_api')
        node.run()

        wallet = node.attach_wallet()

        accounts_and_balances = {
            'first': '100.000 TESTS',
            'second': '200.000 TESTS',
            'third': '300.000 TESTS',
        }

        with wallet.in_single_transaction():
            for account, amount in accounts_and_balances.items():
                wallet.api.create_account('initminer', account, '{}', False)
                wallet.api.transfer('initminer', account, amount, 'memo', False)

        for account, expected_balance in accounts_and_balances.items():
            balance = wallet.api.get_account(account)['result']['balance']
            assert balance == expected_balance
