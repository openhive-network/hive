from __future__ import annotations

import test_tools as tt
from hive_local_tools import run_for
from test_tools.__private.wallet.constants import SimpleTransaction


@run_for("testnet", enable_plugins=["account_history_api"])
def test_is_get_impacted_accounts_operation_collect_accounts_from_the_comment_payout_beneficiaries(
    node: tt.InitNode,
) -> None:
    """
    If get_impacted_accounts_operation collect accounts from the comment_payout_beneficiaries, comment_option operations
    appear in account history of account, which was set a beneficiaries and in setter account.
    """
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.create_account("initminer", "bob", "{}")
    wallet.api.post_comment("initminer", "test-post", "", "test-parent-permlink", "test-title", "test-body", "{}")

    transaction = {
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
                                    {"account": "alice", "weight": 100},
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

    wallet.api.sign_transaction(SimpleTransaction(**transaction))
    node.wait_for_irreversible_block()

    alice_account_history = wallet.api.get_account_history("alice", -1, 100)
    initminer_account_history = wallet.api.get_account_history("initminer", -1, 100)

    assert "comment_options" in str(alice_account_history)
    assert "comment_options" in str(initminer_account_history)
