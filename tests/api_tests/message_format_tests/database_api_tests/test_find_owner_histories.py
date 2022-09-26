import test_tools as tt

from ..local_tools import run_for
from ....local_tools import date_from_now


@run_for('testnet')
def test_find_owner_histories_in_testnet(prepared_node):
    wallet = tt.Wallet(attach_to=prepared_node)
    wallet.create_account('alice', hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    # update_account_auth_key with owner parameter is called to change owner authority history
    wallet.api.update_account_auth_key('alice', 'owner', tt.Account('some key').public_key, 1)
    owner_auths = prepared_node.api.database.find_owner_histories(owner='alice')['owner_auths']
    assert len(owner_auths) != 0


@run_for('mainnet_5m', 'mainnet_64m')
def test_find_owner_histories_in_mainnet(prepared_node):
    owner_auths = prepared_node.api.database.find_owner_histories(owner='temporary_name')['owner_auths']
    assert len(owner_auths) != 0
