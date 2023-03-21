import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_list_witnesses(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        # mark alice as new witness
        wallet.api.update_witness('alice', 'http://url.html', tt.Account('alice').keys.public,
                                  {'account_creation_fee': tt.Asset.Test(28), 'maximum_block_size': 131072,
                                   'hbd_interest_rate': 1000})
    witnesses = node.api.database.list_witnesses(start='', limit=100, order='by_name')['witnesses']
    assert len(witnesses) != 0
