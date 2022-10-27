import test_tools as tt


# HIVE (TEST)
# HBD -- Hive Backed Dollar (TBD)
# VEST (zamrożone HIVE'Y)
# RC -- Resource Credits -- paliwko


def test_if_node_starts_up_correctly():
    node = tt.InitNode()
    node.run()


    wallet = tt.Wallet(attach_to=node)

    wallet.api.create_account("initminer", "alice", "{}")
    print('@@@ ', wallet.api.info())
    print('### ', wallet.api.about())

    # before transfer
    alice = node.api.condenser.get_accounts(["alice"])
    transaction = wallet.api.transfer('initminer', 'alice', tt.Asset.Test(100), "memo", broadcast=False)

    node.api.condenser.broadcast_transaction(transaction)
    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(100))

    # after transferr
    alice = node.api.condenser.get_accounts(["alice"])

    wallet.api.create_account("initminer", "bob", "{}")
    bob = node.api.condenser.get_accounts(["bob"])
    transaction_bob = wallet.api.transfer('alice', 'bob', tt.Asset.Test(9.8), "alice to bob")

    bob = node.api.condenser.get_accounts(["bob"])

    print()
