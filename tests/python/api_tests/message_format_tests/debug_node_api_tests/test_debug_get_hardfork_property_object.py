from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


# Tests for debug_node API can be performed only in testnet
@run_for("testnet")
def test_debug_get_hardfork_property_object(node: tt.InitNode) -> None:
    node.api.debug_node.debug_get_hardfork_property_object()
