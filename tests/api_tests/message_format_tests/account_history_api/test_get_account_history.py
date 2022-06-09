import pytest

import test_tools as tt

from ..local_tools import run_for

ACCOUNT = 'initminer'


@pytest.mark.parametrize(
    'account_name, start, limit', [
        (ACCOUNT, 0, 1),
        (ACCOUNT, 1.1, 1),
        (ACCOUNT, 1, 1.1),
        (ACCOUNT, 1000, 1000),
        (ACCOUNT, -1, 1000),
        (ACCOUNT, '-1', 1000),
        (ACCOUNT, -1, '1000'),
        (ACCOUNT, True, 1),  # bool is treated like numeric (0:1)
        (ACCOUNT, True, True),
        (ACCOUNT, None, True),  # none is treated like numeric (0)
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_account_history(prepared_node, account_name, start, limit):
    prepared_node.api.account_history.get_account_history(
        account=account_name,
        start=start,
        limit=limit,
        include_reversible=True,
    )


@pytest.mark.parametrize(
    'account_name, start, limit', [
        (ACCOUNT, 0, 0),
        (ACCOUNT, 1, 100),
        (ACCOUNT, '1.1', 100),
        (ACCOUNT, 1, '1.1'),
        (ACCOUNT, 1000, 1001),
        (ACCOUNT, 1, ''),
        (ACCOUNT, '', 100),
        (ACCOUNT, 1, 'alice'),
        (ACCOUNT, 'alice', 100),
        ([ACCOUNT], 'alice', 100),
        ({}, 'alice', 100),
        (None, 'alice', 100),
    ]
)
@run_for('testnet', 'mainnet_5m', 'mainnet_64m')
def test_get_account_history_with_incorrect_values(prepared_node, account_name, start, limit):
    with pytest.raises(tt.exceptions.CommunicationError):
        prepared_node.api.account_history.get_account_history(
            account=account_name,
            start=start,
            limit=limit,
            include_reversible=True,
        )
