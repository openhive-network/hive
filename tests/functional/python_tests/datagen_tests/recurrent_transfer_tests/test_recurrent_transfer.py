import test_tools as tt

from hive_local_tools import run_for


@run_for("testnet")
def test_recurrent_transfer(node):
    # Validate if it is possible to set up and send recurrent transfer from one account to another account
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("sender",
                          hives=tt.Asset.Test(100),
                          vests=tt.Asset.Test(100))

    wallet.create_account("receiver")

    # Validate that sender's balance is equal 100 Tests
    assert node.api.wallet_bridge.get_accounts(['sender'])[0]["balance"] == tt.Asset.Test(100)
    # Validate that receiver's balance is equal 0 Tests
    assert node.api.wallet_bridge.get_accounts(['receiver'])[0]["balance"] == tt.Asset.Test(0)

    # Set up and send recurrent transfer
    wallet.api.recurrent_transfer("sender",
                                  "receiver",
                                  tt.Asset.Test(50),
                                  f"recurrent transfer to receiver",
                                  24,
                                  2)

    # Wait 24h
    node.wait_for_irreversible_block()
    node.restart(time_offset="+24h")

    # Validate that sender's balance is equal 0 Tests after 24h
    assert node.api.wallet_bridge.get_accounts(['sender'])[0]["balance"] == tt.Asset.Test(0).as_nai()
    # Validate that receiver's balance is equal 100 (2*50 Tests) after 24h
    assert node.api.wallet_bridge.get_accounts(['receiver'])[0]["balance"] == tt.Asset.Test(100).as_nai()
