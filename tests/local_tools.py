from datetime import datetime, timedelta

from typing import Literal

import pytest


def date_from_now(*, weeks):
    future_data = datetime.now() + timedelta(weeks=weeks)
    return future_data.strftime('%Y-%m-%dT%H:%M:%S')


def run_for(*node_names: Literal['testnet', 'mainnet_5m', 'mainnet_64m']):
    """
    Runs decorated test for each node specified as parameter.

    Each test case is marked with `pytest.mark.<node_name>`, which allow running only selected tests with
    `pytest -m <node_name>`.

    Allows to perform optional, additional preparations. See `should_prepare` fixture for details.
    """
    def __assert_node_is_specified():
        if not node_names:
            raise AssertionError("The @run_for decorator requires at least one argument. "
                                 "Use at least one of the supported nodes, to mark test.")

    __assert_node_is_specified()
    return pytest.mark.parametrize(
        'prepared_node',
        [pytest.param((name,), marks=getattr(pytest.mark, name)) for name in node_names],
        indirect=['prepared_node'],
    )
