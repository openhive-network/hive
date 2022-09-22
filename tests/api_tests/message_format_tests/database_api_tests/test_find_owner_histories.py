import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_find_owner_histories(node, wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    # update_account_auth_key with owner parameter is called to change owner authority history
    wallet.api.update_account_auth_key('alice', 'owner', tt.Account('some key').public_key, 1)
    owner_auths = node.api.database.find_owner_histories(owner='alice')['owner_auths']
    assert len(owner_auths) != 0
