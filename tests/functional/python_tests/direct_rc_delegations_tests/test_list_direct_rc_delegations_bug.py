import pytest

from test_tools import Asset


@pytest.mark.parametrize(
    'from_, to, , expected_delegations', [

        (
                'alice', 'bob',
                [
                    {'from': 'alice', 'to': 'bob', 'delegated_rc': 5},
                    {'from': 'alice', 'to': 'carol', 'delegated_rc': 10}
                ]
        ),
        (
                'alice', '',
                [
                    {'from': 'alice', 'to': 'bob', 'delegated_rc': 5},
                    {'from': 'alice', 'to': 'carol', 'delegated_rc': 10}
                ]
        ),
        (
                'alice', 'carol',
                [{'from': 'alice', 'to': 'carol', 'delegated_rc': 10}]
        ),
    ]
)
def test_list_rc_direct_delegations(node, wallet, from_, to, expected_delegations):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')
        wallet.api.create_account('initminer', 'carol', '{}')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(0.1))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc('alice', ['bob'], 5)
        wallet.api.delegate_rc('alice', ['carol'], 10)

    delegations = node.api.wallet_bridge.list_rc_direct_delegations([from_, to], 100)
    assert len(delegations) == len(expected_delegations)

    for delegation, expected_delegation in zip(delegations, expected_delegations):
        assert delegation == expected_delegation


# @pytest.mark.skip(reason="Tests check for an unresolved problem. Run the tests after fixing the problems.")
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
def test_list_rc_direct_delegations_with_expected_empty_output(node, wallet, from_, to):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')
        wallet.api.create_account('initminer', 'carol', '{}')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(0.1))
    wallet.api.delegate_rc('alice', ['bob'], 5)
    delegations = node.api.wallet_bridge.list_rc_direct_delegations([from_, to], 100)

    assert len(delegations) == 0


# @pytest.mark.skip(reason="Tests check for an unresolved problem. Run the tests after fixing the problems.")
@pytest.mark.parametrize(
    'from_, to', [
        ('alice', 'bob'),
        ('alice', 'andrew'),
    ]
)
def test_list_rc_direct_delegations_with_expected_non_empty_output(node, wallet, from_, to):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')
        wallet.api.create_account('initminer', 'andrew', '{}')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(0.1))
    wallet.api.delegate_rc('alice', ['bob'], 5)
    delegations = node.api.wallet_bridge.list_rc_direct_delegations([from_, to], 100)

    assert len(delegations) != 0