import pytest

from test_tools import Asset, exceptions
from validate_message import validate_message
import schemas

ACCOUNTS = [f'account-{i}' for i in range(3)]

# TODO BUG LIST!
"""
1. Problem with running wallet_bridge_api.find_rc_accounts with incorrect argument type (putting array as 
argument)  (# BUG1) 

2. Problem with running wallet_bridge_api.list_witnesses with incorrect argument type (putting 
bool as limit)  (# BUG2) 

3. Problem with running wallet_bridge_api.list_rc_direct_delegations with incorrect argument 
type (putting bool as limit)  (# BUG3) 
"""


@pytest.mark.parametrize(
    'rc_accounts', [
        [''],
        ['non-exist-acc'],
        [ACCOUNTS[0]],
        ACCOUNTS,
    ]
)
def test_find_rc_accounts_with_correct_value(node, wallet, rc_accounts):
    wallet.create_accounts(len(ACCOUNTS))

    validate_message(
        node.api.wallet_bridge.find_rc_accounts(rc_accounts),
        schemas.find_rc_accounts,
    )


@pytest.mark.parametrize(
    'incorrect_type_argument', [
        [100],  # BUG1
        [True],  # BUG1
        100,
        True,
        'incorrect_string_argument'
    ]
)
def test_find_rc_accounts_with_incorrect_type_of_argument(node, wallet, incorrect_type_argument):
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.find_rc_accounts(incorrect_type_argument)


@pytest.mark.parametrize(
    'rc_account, limit', [
        # RC ACCOUNT
        (ACCOUNTS[0], 100),
        (ACCOUNTS[-1], 100),
        ('non-exist-acc', 100),
        ('', 100),

        # LIMIT
        (ACCOUNTS[0], 0),
        (ACCOUNTS[0], 1000),
    ]
)
def test_list_rc_accounts_with_correct_values(node, wallet, rc_account, limit):
    wallet.create_accounts(len(ACCOUNTS))

    validate_message(
        node.api.wallet_bridge.list_rc_accounts(rc_account, limit),
        schemas.list_rc_accounts
    )


@pytest.mark.parametrize(
    'rc_account, limit', [
        # LIMIT
        (ACCOUNTS[0], -1),
        (ACCOUNTS[0], 1001),
    ]
)
def test_list_rc_accounts_with_incorrect_values(node, wallet, rc_account, limit):
    wallet.create_accounts(len(ACCOUNTS))

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.list_rc_accounts(rc_account, limit),


@pytest.mark.parametrize(
    'rc_account, limit', [
        # WITNESS ACCOUNT
        (100, 100),
        (True, 100),

        # LIMIT
        (ACCOUNTS[0], 'incorrect_string_argument'),
        (ACCOUNTS[0], True),  # BUG2
    ]
)
def test_list_rc_accounts_with_incorrect_type_of_arguments(node, wallet, rc_account, limit):
    wallet.create_accounts(len(ACCOUNTS))
    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.list_rc_accounts(rc_account, limit),


@pytest.mark.parametrize(
    'from_, to, limit', [
        # FROM, TO
        (ACCOUNTS[0], ACCOUNTS[1], 100),
        (ACCOUNTS[0], '', 100),

        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], 0),
        (ACCOUNTS[0], ACCOUNTS[1], 1000),
    ]
)
def test_list_rc_direct_delegations_with_correct_value(node, wallet, from_, to, limit):
    wallet.create_accounts(len(ACCOUNTS))
    wallet.api.transfer_to_vesting('initminer', ACCOUNTS[0], Asset.Test(0.1))
    wallet.api.delegate_rc(ACCOUNTS[0], [ACCOUNTS[1]], 5)

    validate_message(
        node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit),
        schemas.list_rc_direct_delegations
    )


@pytest.mark.parametrize(
    'from_, to, limit', [
        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], -1),
        (ACCOUNTS[0], '', 1001),
    ]
)
def test_list_rc_direct_delegations_with_incorrect_value(node, wallet, from_, to, limit):
    wallet.create_accounts(len(ACCOUNTS))
    wallet.api.transfer_to_vesting('initminer', ACCOUNTS[0], Asset.Test(0.1))
    wallet.api.delegate_rc(ACCOUNTS[0], [ACCOUNTS[1]], 5)

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit),


@pytest.mark.parametrize(
    'from_, to, limit', [
        # FROM_
        (100, ACCOUNTS[1], 100),
        (True, ACCOUNTS[1], 100),

        # TO
        (ACCOUNTS[0], 100, 100),
        (ACCOUNTS[0], True, 100),

        # LIMIT
        (ACCOUNTS[0], ACCOUNTS[1], 'incorrect_string_argument'),
        (ACCOUNTS[0], ACCOUNTS[1], True),  # BUG3
    ]
)
def test_list_rc_direct_delegations_with_incorrect_type_of_arguments(node, wallet, from_, to, limit):
    wallet.create_accounts(len(ACCOUNTS))
    wallet.api.transfer_to_vesting('initminer', ACCOUNTS[0], Asset.Test(0.1))
    wallet.api.delegate_rc(ACCOUNTS[0], [ACCOUNTS[1]], 5)

    with pytest.raises(exceptions.CommunicationError):
        node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit),
