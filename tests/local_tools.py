from typing import Literal

import pytest


def run_for(*node_names: Literal['testnet', 'mainnet_5m', 'mainnet_64m']):
    """
    Runs decorated test for each node specified as parameter.

    Each test case is marked with `pytest.mark.<node_name>`, which allow running only selected tests with
    `pytest -m <node_name>`.

    Allows to perform optional, additional preparations. See `should_prepare` fixture for details.
    """
    return pytest.mark.parametrize(
        'prepared_node',
        [pytest.param((name,), marks=getattr(pytest.mark, name)) for name in node_names],
        indirect=['prepared_node'],
    )
