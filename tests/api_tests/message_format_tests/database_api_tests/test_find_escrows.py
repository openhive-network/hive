import test_tools as tt

from ..local_tools import prepare_escrow, run_for


# This test cannot be performed on 5 million blocklog because it doesn't contain any escrows.
# See the readme.md file in this directory for further explanation.
@run_for('testnet', 'mainnet_64m')
def test_find_escrows(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        prepare_escrow(wallet, sender='alice')

    account = prepared_node.api.database.list_escrows(start=['', 0], limit=5, order='by_from_id')['escrows'][0]['from']
    # "from" is a Python keyword and needs workaround
    escrows = prepared_node.api.database.find_escrows(**{'from': account})['escrows']
    assert len(escrows) != 0
