from datetime import datetime, timedelta
from typing import Literal, Optional

import pytest

from test_tools import Account, Asset, Wallet


def create_account_and_fund_it(wallet: Wallet, name: str, creator: str = 'initminer', tests: Optional[Asset.Test] = None,
                               vests: Optional[Asset.Test] = None, tbds: Optional[Asset.Tbd] = None):
    assert any(asset is not None for asset in [tests, vests, tbds]), 'You forgot to fund account'

    account = Account(name)
    create_account_transaction = wallet.api.create_account_with_keys(creator, account.name, '{}',
        account.public_key, account.public_key, account.public_key, account.public_key)
    wallet.api.import_key(account.private_key)

    with wallet.in_single_transaction():
        if tests is not None:
            wallet.api.transfer(creator, name, tests, 'memo')

        if vests is not None:
            wallet.api.transfer_to_vesting(creator, name, vests)

        if tbds is not None:
            wallet.api.transfer(creator, name, tbds, 'memo')

    return create_account_transaction


def date_from_now(*, weeks):
    future_data = datetime.now() + timedelta(weeks=weeks)
    return future_data.strftime('%Y-%m-%dT%H:%M:%S')


def replay_prepared_block_log(func):
    """
    Use of a decorator allows you to use the available a local block log. Each test case is marked with
     `pytest.mark.replay_prepared_block_log`,

    Definition local block_log is available here:
      /hive/tests/api_tests/message_format_tests/wallet_bridge_api_tests/block_log/generate_block_log.py

    Usage:
    @replay_prepared_block_log
    @run_for('testnet', 'mainnet_5m')
    def test_with_available_block_log(prepared_node):
        pass
    """
    return pytest.mark.replay_prepared_block_log(func)


def run_for(*node_names: Literal['testnet', 'mainnet_5m', 'mainnet_64m']):
    """
    Runs decorated test for each node specified as parameter.

    Each test case is marked with `pytest.mark.<node_name>`, which allow running only selected tests with
    `pytest -m <node_name>`.

    Allows to perform optional, additional preparations. See `should_prepare` fixture for details.
    """
    return pytest.mark.parametrize(
        'node',
        [pytest.param((name,), marks=getattr(pytest.mark, name)) for name in node_names],
        indirect=['node'],
    )
