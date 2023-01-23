import test_tools as tt

from hive_local_tools import run_for


@run_for("testnet")
def test_cancellation_of_a_recurring_transfer(node):
    # To cancel recurrent transfer make this transfer with hive amount equals 0
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account('sender',
                          hives=tt.Asset.Test(100),
                          vests=tt.Asset.Test(100))

    wallet.create_account('receiver')

    wallet.api.recurrent_transfer('sender',
                                  'receiver',
                                  tt.Asset.Test(0.001),
                                  f'recurrent transfer to receiver',
                                  24,
                                  2)
    assert len(wallet.api.find_recurrent_transfers('sender')) == 1

    wallet.api.recurrent_transfer('sender',
                                  'receiver',
                                  tt.Asset.Test(0),
                                  f'recurrent transfer to receiver',
                                  24,
                                  2)
    assert len(wallet.api.find_recurrent_transfers('sender')) == 0