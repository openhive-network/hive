from __future__ import annotations

import time

import pytest
from beekeepy.exceptions import CommunicationError

import test_tools as tt
from hive_local_tools.functional.python.operation.withdrawe_vesting import PowerDownAccount


def _create_and_run_node(witnesses: list[str]) -> tt.InitNode:
    """Create and start a new InitNode with the given witnesses."""
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")

    for witness in witnesses:
        node.config.witness.append(witness)
        node.config.private_key.append(tt.PrivateKey(witness))

    node.run(
        time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5),
        alternate_chain_specs=tt.AlternateChainSpecs(
            genesis_time=int(tt.Time.now(serialize=False).timestamp()),
            hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=0)],
            init_supply=410_000_000_000,
            hbd_init_supply=30_000_000_000,
            initial_vesting=tt.InitialVesting(vests_per_hive=1800, hive_amount=170_000_000_000),
            init_witnesses=witnesses,
        ),
    )
    return node


@pytest.fixture()
def prepared_node() -> tt.InitNode:
    witnesses = [f"witness-{num}" for num in range(20)]

    # Retry node startup to handle intermittent CommunicationError during port discovery.
    # When running parallel tests, nodes may bind ports but not be ready to accept
    # connections immediately, causing beekeepy's get_app_status call to fail.
    max_retries = 3
    for attempt in range(max_retries):
        try:
            return _create_and_run_node(witnesses)
        except CommunicationError:
            if attempt == max_retries - 1:
                raise
            tt.logger.warning(f"Node startup failed (attempt {attempt + 1}/{max_retries}), retrying...")
            time.sleep(1)

    # This should never be reached, but satisfies type checker
    raise RuntimeError("Failed to start node after all retries")


@pytest.fixture()
def wallet(prepared_node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=prepared_node)


@pytest.fixture()
def alice(prepared_node, wallet):
    wallet.create_account("alice")
    alice = PowerDownAccount("alice", prepared_node, wallet)
    alice.fund_vests(tt.Asset.Test(13_000))
    alice.update_account_info()
    return alice
