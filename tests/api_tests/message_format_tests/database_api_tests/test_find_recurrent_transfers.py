import test_tools as tt

from ..local_tools import run_for


# This test cannot be performed on 5 million blocklog because it doesn't contain any recurrent transfers - they were
# introduced in HF25, after block with number 5000000. In this case testing on recent (current) blocklog is problematic
# too. There is no listing method for recurrent_transfers, and they could run out of executions after some time.
# See the readme.md file in this directory for further explanation.
@run_for('testnet')
def test_find_recurrent_transfers(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.create_account('initminer', 'bob', '{}')
    # create transfer from alice to bob for amount 5 test hives every 720 hours, repeat 12 times
    wallet.api.recurrent_transfer('alice', 'bob', tt.Asset.Test(5), 'memo', 720, 12)
    # "from" is a Python keyword and needs workaround
    recurrent_transfers = prepared_node.api.database.find_recurrent_transfers(**{'from': 'alice'})['recurrent_transfers']
    assert len(recurrent_transfers) != 0
