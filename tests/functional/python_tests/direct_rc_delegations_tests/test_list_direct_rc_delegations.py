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
        # Empty 'to' parameter allow list all delegations from specific account.
        (
                'alice', '',
                [
                    {'from': 'alice', 'to': 'bob', 'delegated_rc': 5},
                    {'from': 'alice', 'to': 'carol', 'delegated_rc': 10}
                ]
        ),
        # Alice as 'to' parameter allow list delegations to bob and carol, because id of alice is less then id
        # bob's and carol's.
        (
                'alice', 'alice',
                [
                    {'from': 'alice', 'to': 'bob', 'delegated_rc': 5},
                    {'from': 'alice', 'to': 'carol', 'delegated_rc': 10}
                ]
        ),
        # Initminer as 'to' parameter allow list delegations to bob and carol, because id of initminer is less then id
        # bob's and carol's (initminer was created first).
        (
                'alice', 'initminer',
                [
                    {'from': 'alice', 'to': 'bob', 'delegated_rc': 5},
                    {'from': 'alice', 'to': 'carol', 'delegated_rc': 10}
                ]
        ),
        # This case isn't return any delegations, because id of dan is bigger then id bob's and carol's.
        ('alice', 'dan', []),
        # Those cases aren't return any delegations, because weren't any delegations from bob, carol and initminer.
        ('bob', 'alice', []),
        ('bob', 'bob', []),
        ('bob', 'carol', []),
        ('bob', 'dan', []),
        ('bob', 'initminer', []),
        ('carol', 'alice', []),
        ('initminer', 'alice', []),
        ('dan', 'alice', []),
    ]
)
def test_list_rc_direct_delegations(node, wallet, from_, to, expected_delegations):
    # 'to' parameter is name of account, but accounts are listing by id involved with account, NOT alphabetically
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')
        wallet.api.create_account('initminer', 'carol', '{}')
        wallet.api.create_account('initminer', 'dan', '{}')

    wallet.api.transfer_to_vesting('initminer', 'alice', Asset.Test(0.1))

    with wallet.in_single_transaction():
        wallet.api.delegate_rc('alice', ['bob'], 5)
        wallet.api.delegate_rc('alice', ['carol'], 10)

    delegations = wallet.api.list_rc_direct_delegations([from_, to], 100)
    assert len(delegations) == len(expected_delegations)

    for delegation, expected_delegation in zip(delegations, expected_delegations):
        assert delegation == expected_delegation
