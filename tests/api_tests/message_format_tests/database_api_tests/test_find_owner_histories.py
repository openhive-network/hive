import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_find_owner_histories(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
        # update_account_auth_key with owner parameter is called to change owner authority history
        wallet.api.update_account_auth_key('alice', 'owner', tt.Account('some key').public_key, 1)
    account = node.api.database.list_owner_histories(start=['', tt.Time.from_now(weeks=-100)],
                                                     limit=100)['owner_auths'][0]['account']
    owner_auths = node.api.database.find_owner_histories(owner=account)['owner_auths']
    assert len(owner_auths) != 0
