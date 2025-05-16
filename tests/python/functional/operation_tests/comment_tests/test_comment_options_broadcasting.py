from __future__ import annotations

import test_tools as tt
from test_tools.__private.wallet.constants import SimpleTransaction


def test_nai_format_comment_options_with_beneficiaries(node):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.create_account("initminer", "bob", "{}")
    wallet.api.post_comment("initminer", "test-post", "", "test-parent-permlink", "test-title", "test-body", "{}")

    trx = {
        "ref_block_num": 34005,
        "ref_block_prefix": 3818165156,
        "expiration": "2022-07-04T12:16:06",
        "operations": [
            {
                "type": "comment_options_operation",
                "value": {
                    "author": "initminer",
                    "permlink": "test-post",
                    "max_accepted_payout": {"amount": "100000000", "precision": 3, "nai": "@@000000013"},
                    "percent_hbd": 10000,
                    "allow_votes": True,
                    "allow_curation_rewards": True,
                    "extensions": [
                        {
                            "type": "comment_payout_beneficiaries",
                            "value": {
                                "beneficiaries": [
                                    {"account": "alice", "weight": 1000},
                                    {"account": "bob", "weight": 100},
                                ]
                            },
                        }
                    ],
                },
            },
        ],
        "extensions": [],
        "signatures": [],
    }

    transaction = wallet.api.sign_transaction(SimpleTransaction.parse_builtins(trx), broadcast=False)
    node.api.wallet_bridge.broadcast_transaction(transaction)


def test_legacy_format_comment_options_with_beneficiaries(node):
    wallet = tt.OldWallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.create_account("initminer", "bob", "{}")
    wallet.api.post_comment("initminer", "test-post", "", "test-parent-permlink", "test-title", "test-body", "{}")

    trx = {
        "ref_block_num": 34005,
        "ref_block_prefix": 3818165156,
        "expiration": "2022-07-04T12:16:06",
        "operations": [
            [
                "comment_options",
                {
                    "author": "initminer",
                    "permlink": "test-post",
                    "max_accepted_payout": "1000000.000 TBD",
                    "percent_hbd": 10000,
                    "allow_votes": True,
                    "allow_curation_rewards": True,
                    "extensions": [
                        [
                            "comment_payout_beneficiaries",
                            {"beneficiaries": [{"account": "alice", "weight": 100}, {"account": "bob", "weight": 100}]},
                        ]
                    ],
                },
            ]
        ],
        "extensions": [],
        "signatures": [],
        "transaction_id": "9c8455cdf0e3ab1ab9a54ce3301848fed27bc820",
        "block_num": 0,
        "transaction_num": 0,
    }

    transaction = wallet.api.sign_transaction(trx, broadcast=False)
    node.api.wallet_bridge.broadcast_transaction(transaction)
