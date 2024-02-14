from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt
from hive_local_tools.functional.python.hf28.constants import PROXY_ACCOUNT, VOTER_ACCOUNT


@pytest.fixture(scope="session")
def block_log() -> tt.BlockLog:
    """Artifacts must be generated before running tests in parallel to avoid race conditions."""
    block_log_directory = Path(__file__).parent.joinpath("block_log")
    block_log = tt.BlockLog(block_log_directory / "block_log")
    block_log.generate_artifacts()
    return block_log


@pytest.fixture()
def prepare_environment(node: tt.InitNode) -> tuple[tt.InitNode, tt.Wallet]:
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    node.config.plugin.append("condenser_api")
    node.run(time_offset="+0 x5")
    wallet = tt.Wallet(attach_to=node)

    # price stabilization prevents zero payout for comment votes.
    node.set_vest_price(tt.Asset.Vest(1800))

    wallet.create_account(VOTER_ACCOUNT, vests=tt.Asset.Test(10))
    wallet.create_account(PROXY_ACCOUNT)

    return node, wallet


@pytest.fixture()
def prepare_environment_on_hf_27(block_log: tt.BlockLog, node: tt.InitNode) -> tuple[tt.InitNode, tt.Wallet]:
    # run on a node with a date earlier than the start date of hardfork 28 (february 8, 2023 1:00:00 am)
    node = tt.WitnessNode(witnesses=[f"witness{i}-alpha" for i in range(20)])
    absolute_start_time = block_log.get_head_block_time() - tt.Time.seconds(5)
    time_offset = tt.Time.serialize(absolute_start_time, format_=tt.TimeFormats.TIME_OFFSET_FORMAT)
    node.run(
        replay_from=block_log,
        time_offset=time_offset,
    )

    wallet = tt.Wallet(attach_to=node)
    wallet.create_account(VOTER_ACCOUNT, vests=tt.Asset.Test(10))
    wallet.create_account(PROXY_ACCOUNT)

    assert node.api.wallet_bridge.get_hardfork_version() == "1.27.0"
    return node, wallet
