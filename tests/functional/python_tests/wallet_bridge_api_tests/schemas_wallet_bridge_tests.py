from test_tools import Asset

import local_schemas
from schemas.package.validate_message import validate_message


def get_accounts_name(accounts):
    return [account.name for account in accounts]


def test_get_version_pattern(node, wallet):
    validate_message(
        node.api.wallet_bridge.get_version(),
        local_schemas.get_version,
    )


def test_get_hardfork_version_pattern(node, wallet):
    import logging
    import sys
    logging.getLogger().setLevel(logging.DEBUG)
    logging.getLogger().addHandler(logging.StreamHandler(sys.stdout))

    validate_message(
        node.api.wallet_bridge.get_hardfork_version(),
        local_schemas.get_hardfork_version,
    )


def test_get_active_witnesses_pattern(node, wallet):
    validate_message(
        node.api.wallet_bridge.get_active_witnesses(),
        local_schemas.get_active_witnesses,
    )


def test_get_account_pattern(node, wallet):
    initminer = node.api.wallet_bridge.get_account('initminer')
    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(0.1))
    alice = node.api.wallet_bridge.get_account('alice')

    validate_message(
        alice,
        local_schemas.get_account
    )


# TODO: WIP
def test_get_account_history_with_correct_value(node, wallet):
    accounts = get_accounts_name(wallet.create_accounts(2, 'account'))

    node.wait_number_of_blocks(21)  # wait 21 block to appear tranastions in 'get account history'
    response = node.api.wallet_bridge.get_account_history('initminer', -1, 10)
    print()
    validate_message(
        node.api.wallet_bridge.get_account_history(accounts[0], -1, 10),
        local_schemas.account_history
    )


def test_schema(node, wallet):
    # accounts = get_accounts_name(wallet.create_accounts(5, 'account'))
    # wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.1))
    # wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)
    # x = node.api.wallet_bridge.list_rc_accounts('', 100)

    example_data = {'1': 2, 'abc': 'abc'}
    se = [1, 2, 3, 4]
    # example_int = 3

    example_schema = {
        "schema": {
            "sequence": [
                {
                    "type": "int"
                }
            ]
        }
    }

    validate_message(
        se,
        example_schema,
    )
