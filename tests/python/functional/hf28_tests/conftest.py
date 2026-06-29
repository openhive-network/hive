from __future__ import annotations

import time
from pathlib import Path

import pytest
from test_tools.exceptions import CommunicationError, FailedToStartExecutableError
from loguru import logger

import test_tools as tt
from hive_local_tools.functional.python.hf28.constants import (
    PROXY_ACCOUNT,
    VOTER_ACCOUNT,
)
from hive_local_tools.functional.python.operation import Account


@pytest.fixture()
def prepare_environment(node) -> tuple[tt.InitNode, tt.Wallet]:  # noqa: ARG001
    # Note: 'node' parameter is required by @run_for decorator but unused - we create our own node
    max_retries = 3
    for attempt in range(max_retries):
        init_node = tt.InitNode()
        init_node.config.plugin.append("account_history_api")
        init_node.config.plugin.append("condenser_api")
        init_node.config.plugin.append("transaction_status_api")
        try:
            init_node.run(
                timeout=120.0, time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5)
            )
            wallet = tt.Wallet(attach_to=init_node)
            # price stabilization prevents zero payout for comment votes.
            init_node.set_vest_price(tt.Asset.Vest(1800))
            return init_node, wallet
        except (FailedToStartExecutableError, CommunicationError, TimeoutError):
            if attempt < max_retries - 1:
                logger.warning(
                    f"Node startup failed (attempt {attempt + 1}/{max_retries}), retrying..."
                )
                time.sleep(1)
            else:
                raise
    raise RuntimeError("Failed to start node after retries")  # Should not reach here


@pytest.fixture()
def prepare_environment_on_hf_27(node) -> tuple[tt.InitNode, tt.Wallet]:  # noqa: ARG001
    # Note: 'node' parameter is required by @run_for decorator but unused - we create our own node
    # run on a node with a date earlier than the start date of hardfork 28 (february 8, 2023 1:00:00 am)
    max_retries = 3
    for attempt in range(max_retries):
        witness_node = tt.WitnessNode(
            witnesses=[f"witness{i}-alpha" for i in range(20)]
        )
        witness_node.config.block_log_split = -1

        block_log_directory = Path(__file__).parent / "block_log"
        block_log = tt.BlockLog(block_log_directory, "auto")
        # The block log is part of an alternate chain (HF27 active, HF28 scheduled by block number);
        # it must be replayed with the very same chain spec the generator used.
        alternate_chain_specs = tt.AlternateChainSpecs.parse_file(
            block_log_directory / tt.AlternateChainSpecs.FILENAME
        )

        try:
            witness_node.run(
                replay_from=block_log,
                alternate_chain_specs=alternate_chain_specs,
                time_control=tt.StartTimeControl(start_time="head_block_time"),
                timeout=180.0,
            )

            wallet = tt.Wallet(attach_to=witness_node)
            wallet.create_account(VOTER_ACCOUNT, vests=tt.Asset.Test(10))
            wallet.create_account(PROXY_ACCOUNT)
            assert witness_node.api.wallet_bridge.get_hardfork_version() == "1.27.0"
            return witness_node, wallet
        except (FailedToStartExecutableError, CommunicationError, TimeoutError):
            if attempt < max_retries - 1:
                logger.warning(
                    f"Node startup failed (attempt {attempt + 1}/{max_retries}), retrying..."
                )
                time.sleep(1)
            else:
                raise
    raise RuntimeError("Failed to start node after retries")  # Should not reach here


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
