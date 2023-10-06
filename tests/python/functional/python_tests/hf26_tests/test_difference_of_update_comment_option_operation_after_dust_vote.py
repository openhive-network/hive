import test_tools as tt

"""
A bug has been detected in hardfork 25. Votes with weight below the dust_threshold (called "dust votes"),
can be created when were send from an account with very low resources or have low weight. On HF25 they were ignored.
"""


def test_modify_beneficiaries_after_dust_vote_on_hf26(wallet_hf26):
    for account in ["alice", "bob"]:
        wallet_hf26.api.create_account("initminer", account, "{}")

    wallet_hf26.create_account("carol", vests=tt.Asset.Test(1))

    wallet_hf26.api.post_comment("initminer", "test-post", "", "test-parent-permlink", "test-title", "test-body", "{}")
    wallet_hf26.api.vote("carol", "initminer", "test-post", 1)  # "dust-vote"

    assert not is_dust_vote_ignored(wallet_hf26)


def test_modify_beneficiaries_after_dust_vote_on_hf25(wallet_hf25):
    for account in ["alice", "bob"]:
        wallet_hf25.api.create_account("initminer", account, "{}")

    wallet_hf25.create_account("carol", vests=tt.Asset.Test(1))

    wallet_hf25.api.post_comment("initminer", "test-post", "", "test-parent-permlink", "test-title", "test-body", "{}")
    wallet_hf25.api.vote("carol", "initminer", "test-post", 1)  # "dust-vote"

    assert is_dust_vote_ignored(wallet_hf25)


def is_dust_vote_ignored(wallet):
    """
    The hive algorithm works in such a way that "comment_options"  can be changed only, when there
    was no vote for the comment. Changing 'comment options' allows you to verify that vote is seen by hived.
    """

    transaction_containing_comment_option_operation = {
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

    try:
        wallet.api.sign_transaction(transaction_containing_comment_option_operation)
        return True
    except tt.exceptions.CommunicationError:
        return False
