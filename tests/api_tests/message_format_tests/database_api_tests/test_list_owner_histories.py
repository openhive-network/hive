import test_tools as tt

from hive_local_tools import run_for


@run_for('testnet', 'mainnet_5m', 'live_mainnet')
def test_list_owner_histories(node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=node)
        wallet.create_account('alice', vests=tt.Asset.Test(100))
        # update_account_auth_key with owner parameter is called to change owner authority history
        wallet.api.update_account_auth_key('alice', 'owner', tt.Account('some key').keys.public, 1)
    owner_auths = node.api.database.list_owner_histories(start=['', tt.Time.from_now(weeks=-100)],
                                                                  limit=100)['owner_auths']
    assert len(owner_auths) != 0
