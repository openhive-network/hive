import pytest

from test_tools import Asset

@pytest.mark.skip(reason="Tests check for an unresolved problem. Run the tests after fixing the problems.")
@pytest.mark.parametrize(
    'from_, to', [
        ('alice', 'bob'),
        ('alice', ''),
    ]
)
def test_list_rc_direct_delegations_with_existing_delegations(node, wallet, from_, to):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')
        wallet.api.create_account('initminer', 'carol', '{}')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(0.1))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc('alice', ['bob'], 5)
        wallet.api.delegate_rc('alice', ['carol'], 10)

    response = node.api.wallet_bridge.list_rc_direct_delegations([from_, to], 100),

    assert response[0][0]['from'] == 'alice'
    assert response[0][0]['to'] == 'bob'
    assert response[0][0]['delegated_rc'] == 5

    if to != 'bob':
        assert response[0][1]['from'] == 'alice'
        assert response[0][1]['to'] == 'carol'
        assert response[0][1]['delegated_rc'] == 10


@pytest.mark.skip(reason="Tests check for an unresolved problem. Run the tests after fixing the problems.")
@pytest.mark.parametrize(
    'from_, to', [
        ('alice', 'carol'),
        ('alice', 'initminer'),
        ('bob', 'carol'),
        ('bob', 'initminer'),
        ('carol', 'initminer'),
        ('bob', 'alice'),
        ('carol', 'alice'),
        ('initminer', 'alice'),
        ('carol', 'bob'),
        ('initminer', 'bob'),
        ('initminer', 'carol'),
    ]
)
def test_list_rc_direct_delegations_without_existing_delegations(node, wallet, from_, to):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')
        wallet.api.create_account('initminer', 'carol', '{}')
        
    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(0.1))
    wallet.api.delegate_rc('alice', ['bob'], 5)
    response = node.api.wallet_bridge.list_rc_direct_delegations([from_, to], 100),

    assert len(response[0]) == 0
