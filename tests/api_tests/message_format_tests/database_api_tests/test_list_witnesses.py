import test_tools as tt

from ..local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_witnesses(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        # mark alice as new witness
        wallet.api.update_witness('alice', 'http://url.html', tt.Account('alice').public_key,
                                  {'account_creation_fee': tt.Asset.Test(28), 'maximum_block_size': 131072,
                                   'hbd_interest_rate': 1000})
    witnesses = prepared_node.api.database.list_witnesses(start='', limit=100, order='by_name')['witnesses']
    assert len(witnesses) != 0
