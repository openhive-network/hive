from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from functools import partial
from pathlib import Path
from typing import TYPE_CHECKING, Final, Literal

import pytest

if TYPE_CHECKING:
    import tests_tools as tt

PYTHON_TESTS_DIR: Final[Path] = Path(__file__).parent.parent.parent


def run_for(*node_names: Literal["testnet", "mainnet_5m", "live_mainnet"], enable_plugins: list | None = None):
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


def simultaneous_node_startup(
    nodes: list[tt.AnyNode],
    timeout: int,
    alternate_chain_specs: tt.AlternateChainSpecs,
    arguments: list,
    wait_for_live: bool,
    time_control: tt.StartTimeControl = None,
    exit_before_synchronization: bool = False,
):
    """
    A function that starts up multiple nodes simultaneously using ThreadPoolExecutor.
    """
    with ThreadPoolExecutor(max_workers=len(nodes)) as executor:
        tasks = []
        for node in nodes:
            tasks.append(
                executor.submit(
                    partial(
                        lambda _node: _node.run(
                            timeout=timeout,
                            alternate_chain_specs=alternate_chain_specs,
                            arguments=arguments,
                            wait_for_live=wait_for_live,
                            time_control=time_control,
                            exit_before_synchronization=exit_before_synchronization,
                        ),
                        node,
                    )
                )
            )
        for thread_number in tasks:
            thread_number.result()
