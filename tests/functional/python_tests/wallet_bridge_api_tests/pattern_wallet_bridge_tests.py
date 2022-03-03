from pykwalify.core import Core
import pytest

from test_tools import Asset, logger, Wallet, World
import test_tools.exceptions

import schemas
import partial_schemas



def validate_message(message, schema):
    return Core(source_data=message, schema_data=schema, extensions=[partial_schemas.__file__]).validate(raise_exception=True)


def get_accounts_name(accounts):
    return [account.name for account in accounts]


def test_get_version_pattern(node, wallet):
    validate_message(
        node.api.wallet_bridge.get_version(),
        schemas.get_version,
    )


def test_get_hardfork_version_pattern(node, wallet):
    import logging
    import sys
    logging.getLogger().setLevel(logging.DEBUG)
    logging.getLogger().addHandler(logging.StreamHandler(sys.stdout))

    validate_message(
        node.api.wallet_bridge.get_hardfork_version(),
        schemas.get_hardfork_version,
    )


def test_get_active_witnesses_pattern(node, wallet):
    validate_message(
        node.api.wallet_bridge.get_active_witnesses(),
        schemas.get_active_witnesses,
    )


def test_get_account_pattern(node, wallet):
    initminer = node.api.wallet_bridge.get_account('initminer')
    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(0.1))
    alice = node.api.wallet_bridge.get_account('alice')

    validate_message(
        schemas.test_response_data['result'],
        schemas.get_account
    )
    # validate_message(
    #     alice,
    #     schemas.get_account
    # )


def test():
    example_data_str = '123'
    example_data_int = 231

    # validate_message(
    #     example_data_int,
    #     schemas.example
    # )

    validate_message(
        example_data_str,
        schemas.example
    )


def test_asset(node, wallet):
    accounts = get_accounts_name(wallet.create_accounts(5, 'account'))
    wallet.api.transfer_to_vesting('initminer', accounts[0], Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 10)
    x = node.api.wallet_bridge.list_rc_accounts('', 100)

    list_rc_accounts = {
        'type': 'seq',
        'sequence': [
            {
                'type': 'map',
                'mapping': {
                    'account': {'type': 'str'},
                    'rc_manabar': {
                        'type': 'map',
                        'mapping': {
                            'current_mana': {'type': 'int'},
                            'last_update_time': {'type': 'int'},
                        }
                    },
                    'max_rc_creation_adjustment':
                        schemas.AssetVests(),
                    'max_rc': {'type': 'int'},
                    'delegated_rc': {'type': 'int'},
                    'received_delegated_rc': {'type': 'int'},
                }
            }
        ]
    }
    validate_message(
        node.api.wallet_bridge.list_rc_accounts('', 100),
        list_rc_accounts
    )


