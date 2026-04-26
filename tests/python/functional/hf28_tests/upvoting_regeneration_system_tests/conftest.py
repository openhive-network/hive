from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import Account


@pytest.fixture()
def prepare_environment(request: pytest.FixtureRequest) -> tuple[tt.InitNode, tt.Wallet]:
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    node.config.block_log_split = -1
    block_log_directory = Path(__file__).parent.joinpath("block_log")
    block_log = tt.BlockLog(block_log_directory, "auto")

    with open(block_log_directory / "genesis_time", encoding="utf-8") as file:
        genesis_time = file.read()

    if request.getfixturevalue("hardfork_version") != "current":
        hardfork_number = request.getfixturevalue("hardfork_version")
        hardfork_schedule = [
            tt.HardforkSchedule(hardfork=hardfork_number, block_num=1),
            tt.HardforkSchedule(hardfork=hardfork_number + 1, block_num=2000),
        ]
    else:
        # Pin the "current" hardfork to HF28 — the latest the saved block_log can replay
        # under, since its witness extensions were signed at HIVE_BLOCKCHAIN_VERSION 1.28.
        # Reading blockchain_version from the binary breaks once that constant is bumped
        # past 1.28 (e.g. for HF29): the schedule jumps to HF29 at block 1 and the chain
        # rejects the replay via the witness running_version check. Regenerate the
        # block_log when a future hardfork should be the new "current".
        hardfork_schedule = [tt.HardforkSchedule(hardfork=28, block_num=1)]

    node.run(
        time_control=tt.StartTimeControl(start_time="head_block_time", speed_up_rate=10),
        replay_from=block_log,
        alternate_chain_specs=tt.AlternateChainSpecs(
            genesis_time=int(genesis_time),
            hardfork_schedule=hardfork_schedule,
        ),
    )

    wallet = tt.Wallet(attach_to=node)
    wallet.api.import_key(tt.PrivateKey("alice"))

    tt.logger.info(f"Test uses hardfork : {node.api.database.get_hardfork_properties().current_hardfork_version}")

    return node, wallet


@pytest.fixture()
def alice(prepare_environment) -> Account:
    node, wallet = prepare_environment
    alice = Account("alice", node, wallet)
    alice.update_account_info()
    return alice
