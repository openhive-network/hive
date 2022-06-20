import pytest

import test_tools as tt

from .local_tools import as_string, run_for

TESTNET_ACCOUNTS = [f'account-{i}' for i in range(3)]

MAINNET_ACCOUNTS = [
    "gtg",
    "gthibode",
]

CORRECT_VALUES = [
        # FROM_, TO
        (TESTNET_ACCOUNTS[0], TESTNET_ACCOUNTS[1], 100),
        (TESTNET_ACCOUNTS[0], '', 100),

        # LIMIT
        (TESTNET_ACCOUNTS[0], TESTNET_ACCOUNTS[1], 0),
        (TESTNET_ACCOUNTS[0], TESTNET_ACCOUNTS[1], 1000),
]


@pytest.mark.parametrize(
    'from_, to, limit', [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
        (TESTNET_ACCOUNTS[0], TESTNET_ACCOUNTS[1], True),  # bool is treated like numeric (0:1)
    ]
)
@run_for('testnet')
def test_list_rc_direct_delegations_with_correct_value_in_testnet(prepared_node, from_, to, limit):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts=TESTNET_ACCOUNTS)

    result = prepared_node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)
    if len(result) != 0:
        assert len(result) > 0


@pytest.mark.skip(reason='Method will be available after hardfork 1.26')
@run_for('mainnet_5m', 'mainnet_64m')
def test_list_rc_direct_delegations_with_correct_value_mainnet(prepared_node):
    response = prepared_node.api.wallet_bridge.list_rc_direct_delegations([MAINNET_ACCOUNTS[0], MAINNET_ACCOUNTS[1]], 10)
    assert len(response) > 0


@pytest.mark.parametrize(
    'from_, to, limit', [
        # FROM_
        ('', '', 100),
        ('non-exist-acc', '', 100),
        ('true', '', 100),

        # TO
        (TESTNET_ACCOUNTS[0], 'true', 100),
        (TESTNET_ACCOUNTS[0], 'non-exist-acc', 100),

        # LIMIT
        (TESTNET_ACCOUNTS[0], TESTNET_ACCOUNTS[1], -1),
        (TESTNET_ACCOUNTS[0], '', 1001),
    ]
)
@run_for('testnet')
def test_list_rc_direct_delegations_with_incorrect_value_in_testnet(prepared_node, from_, to, limit):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts=TESTNET_ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


@pytest.mark.parametrize(
    'from_, to, limit', [
        # FROM_
        ('', '', 100),
        ('non-exist-acc', '', 100),

        # TO
        (MAINNET_ACCOUNTS[0], 'non-exist-acc', 100),

        # LIMIT
        (MAINNET_ACCOUNTS[0], MAINNET_ACCOUNTS[1], -1),
        (MAINNET_ACCOUNTS[0], '', 1001),
    ]
)
@run_for('mainnet_5m', 'mainnet_64m')
def test_list_rc_direct_delegations_with_incorrect_value_in_mainnet(prepared_node, from_, to, limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


@pytest.mark.parametrize(
    'from_, to, limit', [
        # FROM_
        ('100', TESTNET_ACCOUNTS[1], 100),
        (100, TESTNET_ACCOUNTS[1], 100),
        (True, TESTNET_ACCOUNTS[1], 100),
        ('', TESTNET_ACCOUNTS[1], 100),
        ([TESTNET_ACCOUNTS[0]], TESTNET_ACCOUNTS[1], 100),

        # TO
        (TESTNET_ACCOUNTS[0], 100, 100),
        (TESTNET_ACCOUNTS[0], '100', 100),
        (TESTNET_ACCOUNTS[0], True, 100),
        (TESTNET_ACCOUNTS[0], [TESTNET_ACCOUNTS[1]], 100),

        # LIMIT
        (TESTNET_ACCOUNTS[0], TESTNET_ACCOUNTS[1], 'incorrect_string_argument'),
        (TESTNET_ACCOUNTS[0], TESTNET_ACCOUNTS[1], 'true'),
        (TESTNET_ACCOUNTS[0], TESTNET_ACCOUNTS[1], [100]),

    ]
)
@run_for('testnet')
def test_list_rc_direct_delegations_with_incorrect_type_of_arguments_in_testnet(prepared_node, from_, to, limit):
    wallet = tt.Wallet(attach_to=prepared_node)
    create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts=TESTNET_ACCOUNTS)

    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


@pytest.mark.parametrize(
    'from_, to, limit', [
        # FROM_
        ('100', MAINNET_ACCOUNTS[1], 100),
        (100, MAINNET_ACCOUNTS[1], 100),
        ('', MAINNET_ACCOUNTS[1], 100),
        ([MAINNET_ACCOUNTS[0]], MAINNET_ACCOUNTS[1], 100),

        # TO
        (MAINNET_ACCOUNTS[0], 100, 100),
        (MAINNET_ACCOUNTS[0], '100', 100),
        (MAINNET_ACCOUNTS[0], [MAINNET_ACCOUNTS[1]], 100),

        # LIMIT
        (MAINNET_ACCOUNTS[0], MAINNET_ACCOUNTS[1], 'incorrect_string_argument'),
        (MAINNET_ACCOUNTS[0], MAINNET_ACCOUNTS[1], 'true'),
        (MAINNET_ACCOUNTS[0], MAINNET_ACCOUNTS[1], [100]),

    ]
)
@run_for('mainnet_5m', 'mainnet_64m')
def test_list_rc_direct_delegations_with_incorrect_type_of_arguments_in_mainnet(prepared_node, from_, to, limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.wallet_bridge.list_rc_direct_delegations([from_, to], limit)


def create_accounts_and_delegate_rc_from_account0_to_account1(wallet, accounts):
    wallet.create_accounts(len(accounts))
    wallet.api.transfer_to_vesting('initminer', accounts[0], tt.Asset.Test(0.1))
    wallet.api.delegate_rc(accounts[0], [accounts[1]], 5)
