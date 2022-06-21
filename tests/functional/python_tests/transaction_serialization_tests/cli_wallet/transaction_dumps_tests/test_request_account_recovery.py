import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_request_account_recovery(wallet):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    with wallet.in_single_transaction():
        wallet.api.create_account('alice', 'bob', '{}')
        wallet.api.transfer_to_vesting('alice', 'bob', tt.Asset.Test(100))
        wallet.api.transfer('initminer', 'bob', tt.Asset.Test(100), 'memo')

    wallet.api.change_recovery_account('alice', 'bob')

    alice_owner_key = get_key('owner', wallet.api.get_account('alice') )

    authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[alice_owner_key, 1]]}

    wallet.api.request_account_recovery('alice', 'bob', authority)


def get_key(node_name, result):
    _key_auths = result[node_name]['key_auths']
    assert len(_key_auths) == 1
    __key_auths = _key_auths[0]
    assert len(__key_auths) == 2
    return __key_auths[0]
