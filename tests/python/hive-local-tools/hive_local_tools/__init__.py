from __future__ import annotations

import json
from pathlib import Path
from typing import Final, Literal

import pytest

import test_tools as tt

from .constants import ALTERNATE_CHAIN_JSON_FILENAME

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


def create_alternate_chain_spec_file(
    genesis_time: int | str,
    hardfork_schedule: list[dict[int, int]],
    *,
    init_supply: int | None = None,
    hbd_init_supply: int | None = None,
    initial_vesting: dict[int, int] | None = None,
    init_witnesses: list | None = None,
    hive_owner_update_limit: int | None = None,
) -> None:
    try:
        genesis_time = int(genesis_time)
    except ValueError as e:
        raise ValueError("`genesis_time`, must be provided as int or str type.") from e

    if isinstance(hardfork_schedule, list):
        for d in hardfork_schedule:
            if "hardfork" in d and "block_num" in d:
                assert isinstance(d["hardfork"], int), "`hardfork` must be provided as <int> value."
                assert isinstance(d["block_num"], int), "`block_num` must be provided as <int> value."
    else:
        raise TypeError("Invalid argument type - `hardfork_schedule`. Expected: [{'hardfork': 27, 'block_num': 1}]")

    alternate_chain_spec_content = {
        "genesis_time": genesis_time,
        "hardfork_schedule": hardfork_schedule,
        **({"init_supply": init_supply} if init_supply else {}),
        **({"hbd_init_supply": hbd_init_supply} if hbd_init_supply else {}),
        **({"initial_vesting": initial_vesting} if initial_vesting else {}),
        **({"init_witnesses": init_witnesses} if init_witnesses else {}),
        **({"hive_owner_update_limit": hive_owner_update_limit} if hive_owner_update_limit else {}),
    }

    directory = tt.context.get_current_directory()
    directory.mkdir(parents=True, exist_ok=True)
    with open(directory / ALTERNATE_CHAIN_JSON_FILENAME, "w") as json_file:
        json.dump(alternate_chain_spec_content, json_file)
