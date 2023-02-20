import test_tools as tt


def test_foo():
    # ARRANGE
    node = tt.InitNode()
    node.run()
    wallet = tt.Wallet(attach_to=node)

    # ACT
    wallet.create_account("alice")
    wallet.create_account("bob")

    # ASSERT
    response = node.api.database.list_accounts(start="", limit=100, order="by_name")
    print(response)
    assert len(response['accounts']) == 6 + 2, "Alice and Bob account was not created"

