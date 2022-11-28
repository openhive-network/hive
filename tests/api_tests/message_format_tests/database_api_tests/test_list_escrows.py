import test_tools as tt

from hive_local_tools import run_for
from hive_local_tools.api.message_format import prepare_escrow


# This test cannot be performed on 5 million blocklog because it doesn't contain any escrows.
# See the readme.md file in this directory for further explanation.
@run_for('testnet', 'live_mainnet')
def test_list_escrows(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        prepare_escrow(wallet, sender='alice')
    escrows = node.api.database.list_escrows(start=['', 0], limit=5, order='by_from_id')['escrows']
    assert len(escrows) != 0
