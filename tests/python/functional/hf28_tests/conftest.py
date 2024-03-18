from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt
from hive_local_tools.functional.python.hf28.constants import PROXY_ACCOUNT, VOTER_ACCOUNT
from hive_local_tools.functional.python.operation import Account


@pytest.fixture()
def prepare_environment(node: tt.InitNode) -> tuple[tt.InitNode, tt.Wallet]:
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    node.config.plugin.append("condenser_api")
    node.run(time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5))
    wallet = tt.Wallet(attach_to=node)

    # price stabilization prevents zero payout for comment votes.
    node.set_vest_price(tt.Asset.Vest(1800))
    return node, wallet


@pytest.fixture()
def prepare_environment_on_hf_27(node: tt.InitNode) -> tuple[tt.InitNode, tt.Wallet]:
    # run on a node with a date earlier than the start date of hardfork 28 (february 8, 2023 1:00:00 am)
    node = tt.WitnessNode(witnesses=[f"witness{i}-alpha" for i in range(20)])

    block_log_directory = Path(__file__).parent / "block_log"
    block_log = tt.BlockLog(block_log_directory / "block_log")

    node.run(
        replay_from=block_log,
        time_control=tt.StartTimeControl(start_time="head_block_time"),
    )

    wallet = tt.Wallet(attach_to=node)
    wallet.create_account(VOTER_ACCOUNT, vests=tt.Asset.Test(10))
    wallet.create_account(PROXY_ACCOUNT)
    assert node.api.wallet_bridge.get_hardfork_version() == "1.27.0"
    return node, wallet


@pytest.fixture()
def voter(prepare_environment: tuple[tt.InitNode, tt.Wallet]) -> Account:
    node, wallet = prepare_environment
    wallet.create_account(VOTER_ACCOUNT, vests=tt.Asset.Test(10))
    return Account(VOTER_ACCOUNT, node, wallet)


@pytest.fixture()
def proxy(prepare_environment: tuple[tt.InitNode, tt.Wallet]) -> Account:
    node, wallet = prepare_environment
    wallet.create_account(PROXY_ACCOUNT)
    return Account(PROXY_ACCOUNT, node, wallet)
