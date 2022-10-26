from datetime import datetime, timedelta

from typing import Literal

import pytest


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
    def test_with_available_block_log(node):
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
        [
            pytest.param(
                (name,),
                marks=[
                    getattr(pytest.mark, name),
                    pytest.mark.decorated_with_run_for,
                ],
            )
            for name in node_names
        ],
        indirect=['node'],
    )
