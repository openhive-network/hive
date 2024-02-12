from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

import beem.exceptions
import pytest
from beem.account import Account

import test_tools as tt

if TYPE_CHECKING:
    from hive_local_tools.functional.python.beem import NodeClientMaker


@pytest.fixture()
def node(chain_id, skeleton_key):
    block_log_directory = Path(__file__).parent / "block_log_mirrornet_1k"
    block_log = tt.BlockLog(block_log_directory / "block_log")

    timestamp = block_log.get_head_block_time() - tt.Time.seconds(5)

    init_node = tt.InitNode()
    init_node.config.private_key = skeleton_key
    init_node.config.plugin.append("account_history_api")
    init_node.config.plugin.append("condenser_api")

    init_node.run(
        time_offset=tt.Time.serialize(timestamp, format_=tt.TimeFormats.TIME_OFFSET_FORMAT),
        wait_for_live=True,
        replay_from=block_log,
        arguments=[f"--chain-id={chain_id}", f"--skeleton-key={skeleton_key}"],
    )

    return init_node


@pytest.mark.mirrornet()
@pytest.mark.parametrize("asset", ["HIVE", "HBD", "VESTS"])
def test_assets(asset: str, node_client: NodeClientMaker):
    # ARRANGE
    node_client = node_client()
    account = Account("initminer", hive_instance=node_client)

    # ACT
    balance = account.get_balance("available", asset)

    # ASSERT
    assert asset in balance.symbol


@pytest.mark.mirrornet()
@pytest.mark.parametrize("asset", ["STEEM", "SBD"])
def test_incorrect_assets(asset: str, node_client: NodeClientMaker):
    # ARRANGE
    node_client = node_client()
    account = Account("initminer", hive_instance=node_client)

    # ACT & ASSERT
    with pytest.raises(beem.exceptions.AssetDoesNotExistsException):
        account.get_balance("available", asset)


@pytest.mark.mirrornet()
def test_hive_transfer(node_client: NodeClientMaker):
    # ARRANGE
    node_client = node_client()

    expected_amount = 1
    new_account_name = "new-account"
    initminer_account = Account("initminer", hive_instance=node_client)

    node_client.create_account(new_account_name, creator="initminer", password="secret")

    # ACT
    initminer_account.transfer(to=new_account_name, amount=expected_amount, asset="HIVE")

    # ASSERT
    new_account = Account(new_account_name, hive_instance=node_client)
    balance = new_account.get_balance("available", "HIVE")

    assert balance.amount == expected_amount
