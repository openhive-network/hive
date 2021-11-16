from test_tools import Account, logger, World, Asset
from utilities import get_key

def test_recovery(wallet):

    wallet.api.create_account('initminer', 'alice', '{}')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(100))

    wallet.api.transfer('initminer', 'alice', Asset.Test(500), 'banana')

    wallet.api.create_account('alice', 'bob', '{}')

    wallet.api.transfer_to_vesting('alice', 'bob', Asset.Test(40))

    wallet.api.transfer('initminer', 'bob', Asset.Test(333), 'kiwi-banana')

    response = wallet.api.change_recovery_account('alice', 'bob')

    _ops = response['operations']
    assert len(_ops) == 1
    assert _ops[0][0] == 'change_recovery_account'
    assert _ops[0][1]['account_to_recover'] == 'alice'
    assert _ops[0][1]['new_recovery_account'] == 'bob'

    alice_owner_key = get_key( 'owner', wallet.api.get_account('alice') )
    bob_owner_key = get_key( 'owner', wallet.api.get_account('bob') )

    authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[alice_owner_key,1]]}

    response = wallet.api.request_account_recovery('alice', 'bob', authority)

    _ops = response['operations']
    assert _ops[0][0] == 'request_account_recovery'
    assert _ops[0][1]['recovery_account'] == 'alice'
    assert _ops[0][1]['account_to_recover'] == 'bob'

    wallet.api.update_account_auth_key('bob', 'owner', bob_owner_key, 3)

    recent_authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[bob_owner_key,1]]}
    new_authority = {"weight_threshold": 1,"account_auths": [], "key_auths": [[alice_owner_key,1]]}

    response = wallet.api.recover_account('bob', recent_authority, new_authority)

    _ops = response['operations']
    assert _ops[0][0] == 'recover_account'
    assert _ops[0][1]['account_to_recover'] == 'bob'

