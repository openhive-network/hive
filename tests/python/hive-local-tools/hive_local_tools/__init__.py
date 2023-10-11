from __future__ import annotations

from pathlib import Path
from typing import Final, Literal

import pytest

PYTHON_TESTS_DIR: Final[Path] = Path(__file__).parent.parent.parent


def run_for(*node_names: Literal["testnet", "mainnet_5m", "live_mainnet"], enable_plugins: list = None):
    """
    Runs decorated test for each node specified as parameter.

    Each test case is marked with `pytest.mark.<node_name>`, which allow running only selected tests with
    `pytest -m <node_name>`.

    Allows to perform optional, additional preparations. See `should_prepare` fixture for details.
    """

    if enable_plugins is None:
        enable_plugins = []

    def __assert_node_is_specified():
        if not node_names:
            raise AssertionError(
                "The @run_for decorator requires at least one argument. "
                "Use at least one of the supported nodes, to mark test."
            )

    __assert_node_is_specified()
    return pytest.mark.parametrize(
        "node",
        [
            pytest.param(
                (name,),
                marks=[
                    getattr(pytest.mark, name),
                    pytest.mark.decorated_with_run_for,
                    pytest.mark.enable_plugins(enable_plugins),
                ],
            )
            for name in node_names
        ],
        indirect=["node"],
    )
