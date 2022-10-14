from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

from beem.account import Account
import beem.exceptions
import pytest

import test_tools as tt

if TYPE_CHECKING:
    from tests.functional.python_tests.dhf_tests.conftest import NodeClientMaker


@pytest.fixture
def node(chain_id, skeleton_key):
    block_log_directory = Path(__file__).parent / 'block_log_mirrornet_1k'
    block_log_path = block_log_directory / "block_log"

    init_node = tt.ApiNode()
    init_node.config.private_key = skeleton_key
    init_node.run(
        wait_for_live=False,
        replay_from=block_log_path,
        arguments=[f"--chain-id={chain_id}", f"--skeleton-key={skeleton_key}"]
    )

    return init_node


@pytest.mark.mirrornet
@pytest.mark.parametrize("asset", ["HIVE", "HBD", "VESTS"])
def test_assets(asset: str, node_client: NodeClientMaker):
    # ARRANGE
    node_client = node_client()
    account = Account("initminer", hive_instance=node_client)

    # ACT
    balance = account.get_balance("available", asset)

    # ASSERT
    assert asset in balance.symbol


@pytest.mark.mirrornet
@pytest.mark.parametrize("asset", ["STEEM", "SBD"])
def test_incorrect_assets(asset: str, node_client: NodeClientMaker):
    # ARRANGE
    node_client = node_client()
    account = Account("initminer", hive_instance=node_client)

    # ACT & ASSERT
    with pytest.raises(beem.exceptions.AssetDoesNotExistsException):
        account.get_balance("available", asset)
