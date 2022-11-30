import test_tools as tt

from ....local_tools import run_for


# Resource credits (RC) were introduced after block with number 5000000, that's why this test is performed only on
# testnet and current mainnet.
@run_for('testnet', 'mainnet_64m')
def test_find_rc_accounts(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('gtg', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    accounts = node.api.rc.find_rc_accounts(accounts=['gtg'])['rc_accounts']
    assert len(accounts) != 0
