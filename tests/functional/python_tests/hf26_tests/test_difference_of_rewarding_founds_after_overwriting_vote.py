import test_tools as tt


def test_getting_rewards_after_vote_overwriting_on_hf25(node_hf25, wallet_hf25):
    perform_test_preparation(node_hf25, wallet_hf25)

    bob = wallet_hf25.api.get_account("bob")

    assert bob["reward_vesting_balance"] == tt.Asset.Vest(0)
    assert bob["reward_vesting_hive"] == tt.Asset.Test(0)


def test_getting_rewards_after_vote_overwriting_on_hf26(node_hf26, wallet_hf26):
    perform_test_preparation(node_hf26, wallet_hf26)

    bob = wallet_hf26.api.get_account("bob")

    assert bob["reward_vesting_balance"] > tt.Asset.Vest(0)
    assert bob["reward_vesting_hive"] > tt.Asset.Test(0)


def perform_test_preparation(node, wallet):
    """
    Prepares to receive reward balances with vote overwriting. Vote overwriting blocks receiving rewards by bob on HF25.
    On HF26 this account gets rewards.
    """
    wallet.create_account("alice", vests=tt.Asset.Test(100000))
    wallet.create_account("bob", vests=tt.Asset.Test(100000))

    wallet.api.post_comment("alice", "permlink", "", "paremt-permlink", "title", "body", "{}")

    wallet.api.vote("bob", "alice", "permlink", 100)
    wallet.api.vote("bob", "alice", "permlink", 90)

    node.wait_for_irreversible_block()

    # Offset 1 hour allow to skip time necessary to receive reward balances.
    wallet.close()
    node.close()
    node.run(time_offset="+1h")
    wallet.run()
