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
                'alice', 'carol',
                [{'from': 'alice', 'to': 'carol', 'delegated_rc': 10}]
        ),
        (
                'alice', '',
                [
                    {'from': 'alice', 'to': 'bob', 'delegated_rc': 5},
                    {'from': 'alice', 'to': 'carol', 'delegated_rc': 10}
                ]
        ),
        # Alice as 'to' parameter allow list delegations to bob and carol, because id of alice is less then id
        # alice's and bob's
        (
                'alice', 'alice',
                [
                    {'from': 'alice', 'to': 'bob', 'delegated_rc': 5},
                    {'from': 'alice', 'to': 'carol', 'delegated_rc': 10}
                ]
        ),
        # Initminer as 'to' parameter allow list delegations to bob and carol, because id of initminer is less then id
        # alice's and bob's  (initminer was created first)
        (
                'alice', 'initminer',
                [
                    {'from': 'alice', 'to': 'bob', 'delegated_rc': 5},
                    {'from': 'alice', 'to': 'carol', 'delegated_rc': 10}
                ]
        ),
        # Those cases aren't return any delegations, because weren't any delegations from bob, carol and initminer
        ('bob', 'alice', []),
        ('bob', 'bob', []),
        ('bob', 'carol', []),
        ('bob', 'initminer', []),
        ('carol', 'alice', []),
        ('carol', 'bob', []),
        ('carol', 'carol', []),
        ('carol', 'initminer', []),
        ('initminer', 'alice', []),
        ('initminer', 'bob', []),
        ('initminer', 'carol', []),
        ('initminer', 'initminer', []),
    ]
)
def test_list_rc_direct_delegations(node, wallet, from_, to, expected_delegations):
    # 'to' parameter is name of account, but accounts are listing by id involved with account, NOT alphabetically
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')
        wallet.api.create_account('initminer', 'carol', '{}')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(0.1))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc('alice', ['bob'], 5)
        wallet.api.delegate_rc('alice', ['carol'], 10)

    delegations = wallet.api.list_rc_direct_delegations([from_, to], 100)
    assert len(delegations) == len(expected_delegations)

    for delegation, expected_delegation in zip(delegations, expected_delegations):
        assert delegation == expected_delegation
