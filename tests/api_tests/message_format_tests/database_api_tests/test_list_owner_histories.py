import test_tools as tt

from ..local_tools import run_for
from ....local_tools import date_from_now


@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_list_owner_histories(prepared_node, should_prepare):
    if should_prepare:
        wallet = tt.Wallet(attach_to=prepared_node)
        wallet.create_account('alice', vests=tt.Asset.Test(100))
        # update_account_auth_key with owner parameter is called to change owner authority history
        wallet.api.update_account_auth_key('alice', 'owner', tt.Account('some key').public_key, 1)
    owner_auths = prepared_node.api.database.list_owner_histories(start=['', date_from_now(weeks=-100)],
                                                                  limit=100)['owner_auths']
    assert len(owner_auths) != 0
