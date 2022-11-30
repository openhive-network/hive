from typing import Literal

import pytest


def run_for(*node_names: Literal['testnet', 'mainnet_5m', 'live_mainnet']):
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
