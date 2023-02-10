import test_tools as tt


def test_is_get_impacted_accounts_operation_collect_accounts_from_the_comment_payout_beneficiaries(node):
    """
    If get_impacted_accounts_operation collect accounts from the comment_payout_beneficiaries, comment_option operations
    appear in account history of account, which was set a beneficiaries and in setter account.
    """
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account('initminer', 'alice', '{}')
    wallet.api.create_account('initminer', 'bob', '{}')
    wallet.api.post_comment('initminer', 'test-post', '', 'test-parent-permlink', 'test-title', 'test-body', '{}')
    block = node.api.wallet_bridge.get_block(2)['block']

    block['transactions'] = {
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
                            {
                                "beneficiaries": [
                                    {
                                        "account": "alice",
                                        "weight": 100
                                    },
                                ]
                            }
                        ]
                    ]
                }
            ]
        ],
        "extensions": [],
        "signatures": [],
        "transaction_id": "9c8455cdf0e3ab1ab9a54ce3301848fed27bc820",
        "block_num": 0,
        "transaction_num": 0
    }
    wallet.api.sign_transaction(block['transactions'], broadcast=True)
    node.wait_for_irreversible_block()

    alice_account_history = wallet.api.get_account_history('alice', -1, 100)
    initminer_account_history = wallet.api.get_account_history('initminer', -1, 100)

    assert 'comment_options' in str(alice_account_history)
    assert 'comment_options' in str(initminer_account_history)
