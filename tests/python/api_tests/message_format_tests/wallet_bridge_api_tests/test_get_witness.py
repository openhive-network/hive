from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy.exceptions import ErrorInResponseError

from hive_local_tools import run_for
from hive_local_tools.api.message_format import as_string
from hive_local_tools.api.message_format.wallet_bridge_api import prepare_node_with_witnesses
from hive_local_tools.api.message_format.wallet_bridge_api.constants import WITNESSES_NAMES

if TYPE_CHECKING:
    import test_tools as tt

CORRECT_VALUES = [
    WITNESSES_NAMES[0],
    WITNESSES_NAMES[-1],
    "non-exist-acc",
    "",
    100,
    True,
]


@pytest.mark.parametrize(
    "witness_account",
    [
        *CORRECT_VALUES,
        *as_string(CORRECT_VALUES),
    ],
)
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_witness_with_correct_value(
    node: tt.InitNode | tt.RemoteNode, witness_account: bool | int | str, should_prepare: bool
) -> None:
    if should_prepare:
        node = prepare_node_with_witnesses(node, WITNESSES_NAMES)
    node.api.wallet_bridge.get_witness(witness_account)


@pytest.mark.parametrize("witness_account", [["example-array"]])
@run_for("testnet", "mainnet_5m", "live_mainnet")
def test_get_witness_with_incorrect_type_of_argument(node: tt.InitNode | tt.RemoteNode, witness_account: list) -> None:
    with pytest.raises(ErrorInResponseError):
        node.api.wallet_bridge.get_witness(witness_account)
