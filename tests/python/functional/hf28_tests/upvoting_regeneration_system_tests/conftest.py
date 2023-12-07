from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt
from hive_local_tools import create_alternate_chain_spec_file
from hive_local_tools.constants import ALTERNATE_CHAIN_JSON_FILENAME


@pytest.fixture()
def prepare_environment(request) -> tt.InitNode:
    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    block_log_directory = Path(__file__).parent.joinpath("block_log")

    with open(block_log_directory / "timestamp", encoding="utf-8") as file:
        timestamp = tt.Time.parse(file.read())

    with open(block_log_directory / "genesis_time", encoding="utf-8") as file:
        genesis_time = file.read()

    if request.getfixturevalue("hardfork_version") != "current":
        hardfork_number = request.getfixturevalue("hardfork_version")
        hardfork_schedule = [
            {"hardfork": hardfork_number, "block_num": 1},
            {"hardfork": hardfork_number + 1, "block_num": 2000},
        ]
    else:
        blockchain_version = int(node.get_version()["version"]["blockchain_version"].split(".")[1])
        hardfork_schedule = [{"hardfork": blockchain_version, "block_num": 1}]

    create_alternate_chain_spec_file(
        genesis_time=genesis_time,
        hardfork_schedule=hardfork_schedule,
    )

    node.run(
        time_offset=f"{tt.Time.serialize(timestamp, format_=tt.TimeFormats.TIME_OFFSET_FORMAT)} x10",
        replay_from=block_log_directory / "block_log",
        arguments=["--alternate-chain-spec", str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME)],
    )

    wallet = tt.Wallet(attach_to=node)
    wallet.api.import_key(tt.PrivateKey("alice"))

    tt.logger.info(f"Test uses hardfork : {node.api.database.get_hardfork_properties().current_hardfork_version}")

    return node, wallet
