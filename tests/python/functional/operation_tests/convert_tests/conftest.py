from __future__ import annotations

import time
from datetime import datetime

import pytest
from beekeepy.exceptions import CommunicationError, FailedToStartExecutableError
from loguru import logger

import test_tools as tt
from python.functional.operation_tests.conftest import ConvertAccount


@pytest.fixture()
def alice(create_node_and_wallet_for_convert_tests: tuple[tt.InitNode, tt.Wallet]) -> ConvertAccount:
    node, wallet = create_node_and_wallet_for_convert_tests
    wallet.create_account("alice", hives=200, hbds=200, vests=50)
    return ConvertAccount("alice", node, wallet)


@pytest.fixture()
def init_witnesses() -> list[str]:
    return [f"witness-{it}" for it in range(20)]


@pytest.fixture()
def alternate_chain_spec(init_witnesses: list[str]) -> tt.AlternateChainSpecs:
    return tt.AlternateChainSpecs(
        genesis_time=int(datetime.now().timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=1)],
        init_witnesses=init_witnesses,
        init_supply=20_000_000_000,
        hbd_init_supply=100_000_000,
        initial_vesting=tt.InitialVesting(vests_per_hive=1800, hive_amount=10_000_000_000),
    )


@pytest.fixture()
def create_node_and_wallet_for_convert_tests(
    init_witnesses: list[str], alternate_chain_spec: tt.AlternateChainSpecs
) -> tuple[tt.InitNode, tt.Wallet]:
    max_retries = 3
    for attempt in range(max_retries):
        node = tt.InitNode()
        node.config.witness.extend(init_witnesses)
        node.config.plugin.append("account_history_api")
        try:
            node.run(
                timeout=120.0,
                time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5),
                alternate_chain_specs=alternate_chain_spec,
            )
            wallet = tt.Wallet(attach_to=node)
            wallet.api.update_witness(
                "initminer",
                "http://url.html",
                tt.Account("initminer").public_key,
                {"account_creation_fee": tt.Asset.Test(3)},
            )
            node.wait_number_of_blocks(43)
            return node, wallet
        except (FailedToStartExecutableError, CommunicationError, TimeoutError):
            if attempt < max_retries - 1:
                logger.warning(f"Node startup failed (attempt {attempt + 1}/{max_retries}), retrying...")
                time.sleep(1)
            else:
                raise
    raise RuntimeError("Failed to start node after retries")
