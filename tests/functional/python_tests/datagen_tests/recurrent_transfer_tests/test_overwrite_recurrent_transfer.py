import test_tools as tt

from hive_local_tools import run_for


@run_for("testnet")
def test_overwrite_recurrent_transfer(node):
    # The second recurrent transfer to the same account is overwriting parameters of the first recurrent transfer
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("sender",
                          hives=tt.Asset.Test(100),
                          vests=tt.Asset.Test(100))

    wallet.create_account("receiver")
    with wallet.in_single_transaction():
        for i in range(2):
            wallet.api.recurrent_transfer("sender",
                                          "receiver",
                                          tt.Asset.Test(0.001),
                                          f"recurrent transfer to receiver",
                                          24+i,
                                          2)

    assert len(wallet.api.find_recurrent_transfers("sender")) == 1
