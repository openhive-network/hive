from pykwalify.core import Core
import pytest

from test_tools import Asset, logger, Wallet, World
import test_tools.exceptions

import schemas


def validate_message(message, schema):
    return Core(source_data=message, schema_data=schema).validate(raise_exception=True)


def get_accounts_name(accounts):
    return [account.name for account in accounts]


def test_get_version_pattern(node, wallet):
    validate_message(
        node.api.wallet_bridge.get_version(),
        schemas.get_version,
    )


def test_get_hardfork_version_pattern(node, wallet):
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
    response = node.api.wallet_bridge.get_account('initminer')
    print()

    get_accounts = {
        'type': 'map',
        'mapping': {
            'id': {'type': 'int'},
            'name': {'type': 'str'},
            'owner': {
                'type': 'map',
                'mapping': {
                    'weight_threshold': {'type': 'int'},
                    'account_auths': {'type': 'sequence'},
                    'key_auths': {'type': 'sequence'},
                }
            },
            'active': {'type': 'any'},
            'posting': {'type': 'any'},
            'memo_key': {'type': 'any'},
            'json_metadata': {'type': 'str'},
            'posting_json_metadata': {'type': 'str'},
            'proxy': {'type': 'str'},
            'previous_owner_update': {'type': 'str'},
            'last_owner_update': {'type': 'str'},
            'last_account_update': {'type': 'str'},
            'created': {'type': 'str'},
            'mined': {'type': 'bool'},
            "recovery_account": {'type': 'str'},
            "last_account_recovery": {'type': 'str'},

            "reset_account": "null",
            "comment_count": 0,
            "lifetime_vote_count": 0,
            "post_count": 0,
            "can_vote": true,
            "voting_manabar": {
                "current_mana": 239067,
                "last_update_time": 1646308062
            },
            "downvote_manabar": {
                "current_mana": 59766,
                "last_update_time": 1646308062
            },
            "balance": {
                "amount": "250000000000",
                "precision": 3,
                "nai": "@@000000021"
            },
            "savings_balance": {
                "amount": "0",
                "precision": 3,
                "nai": "@@000000021"
            },
            "hbd_balance": {
                "amount": "7000000000",
                "precision": 3,
                "nai": "@@000000013"
            },
            "hbd_seconds": "0",
            "hbd_seconds_last_update": "1970-01-01T00:00:00",
            "hbd_last_interest_payment": "1970-01-01T00:00:00",
            "savings_hbd_balance": {
                "amount": "0",
                "precision": 3,
                "nai": "@@000000013"
            },
            "savings_hbd_seconds": "0",
            "savings_hbd_seconds_last_update": "1970-01-01T00:00:00",
            "savings_hbd_last_interest_payment": "1970-01-01T00:00:00",
            "savings_withdraw_requests": 0,
            "reward_hbd_balance": {
                "amount": "0",
                "precision": 3,
                "nai": "@@000000013"
            },
            "reward_hive_balance": {
                "amount": "0",
                "precision": 3,
                "nai": "@@000000021"
            },
            "reward_vesting_balance": {
                "amount": "0",
                "precision": 6,
                "nai": "@@000000037"
            },
            "reward_vesting_hive": {
                "amount": "0",
                "precision": 3,
                "nai": "@@000000021"
            },
            "vesting_shares": {
                "amount": "239067",
                "precision": 6,
                "nai": "@@000000037"
            },
            "delegated_vesting_shares": {
                "amount": "0",
                "precision": 6,
                "nai": "@@000000037"
            },
            "received_vesting_shares": {
                "amount": "0",
                "precision": 6,
                "nai": "@@000000037"
            },
            "vesting_withdraw_rate": {
                "amount": "1",
                "precision": 6,
                "nai": "@@000000037"
            },
            "post_voting_power": {
                "amount": "239067",
                "precision": 6,
                "nai": "@@000000037"
            },
            "next_vesting_withdrawal": "1969-12-31T23:59:59",
            "withdrawn": 0,
            "to_withdraw": 0,
            "withdraw_routes": 0,
            "pending_transfers": 0,
            "curation_rewards": 0,
            "posting_rewards": 0,
            "proxied_vsf_votes": [
                0,
                0,
                0,
                0
            ],
            "witnesses_voted_for": 0,
            "last_post": "1970-01-01T00:00:00",
            "last_root_post": "1970-01-01T00:00:00",
            "last_post_edit": "1970-01-01T00:00:00",
            "last_vote_time": "1970-01-01T00:00:00",
            "post_bandwidth": 0,
            "pending_claimed_accounts": 0,
            "open_recurrent_transfers": 0,
            "is_smt": false,
            "delayed_votes": [

            ],
            "governance_vote_expiration_ts": "1969-12-31T23:59:59"

        }
    }


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
                        schemas.partial_schemas.ASSET_VESTS,
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
