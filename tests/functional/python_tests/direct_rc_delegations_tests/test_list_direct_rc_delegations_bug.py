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
                'alice', 'initminer',
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


@pytest.mark.parametrize(
    'from_, to', [
        ('alice', 'andrew'),
        ('bob', 'andrew'),
        ('bob', 'initminer'),
        ('andrew', 'initminer'),
        ('bob', 'alice'),
        ('andrew', 'alice'),
        ('initminer', 'alice'),
        ('andrew', 'bob'),
        ('initminer', 'bob'),
        ('initminer', 'andrew'),
    ]
)
def test_list_rc_direct_delegations_with_expected_empty_output(node, wallet, from_, to):
    # 'to' parameter is name of account, but accounts are listing by id involved with account, NOT alphabetically
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')
        wallet.api.create_account('initminer', 'andrew', '{}')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(0.1))
    wallet.api.delegate_rc('alice', ['bob'], 5)
    delegations = node.api.wallet_bridge.list_rc_direct_delegations([from_, to], 100)

    assert len(delegations) == 0
