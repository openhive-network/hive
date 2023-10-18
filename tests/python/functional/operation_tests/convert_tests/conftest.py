from __future__ import annotations

import json
from datetime import datetime

import pytest

import test_tools as tt
from hive_local_tools.constants import ALTERNATE_CHAIN_JSON_FILENAME
from python.functional.operation_tests.conftest import ConvertAccount


@pytest.fixture()
def alice(create_node_and_wallet_for_convert_tests) -> ConvertAccount:
    node, wallet = create_node_and_wallet_for_convert_tests
    wallet.create_account("alice", hives=200, hbds=200, vests=50)
    return ConvertAccount("alice", node, wallet)


@pytest.fixture()
def create_node_and_wallet_for_convert_tests() -> tuple(tt.InitNode, tt.Wallet):
    node = tt.InitNode()
    init_witnesses = [f"witness-{it}" for it in range(20)]
    set_initial_parameters_of_blockchain(init_witnesses)
    node.config.witness.extend(init_witnesses)
    node.config.plugin.append("account_history_api")
    node.run(
        time_offset="+0h x5",
        arguments=[
            "--alternate-chain-spec",
            str(tt.context.get_current_directory() / ALTERNATE_CHAIN_JSON_FILENAME),
        ],
    )
    wallet = tt.Wallet(attach_to=node)
    wallet.api.update_witness(
        "initminer", "http://url.html", tt.Account("initminer").public_key, {"account_creation_fee": tt.Asset.Test(3)}
    )

    node.wait_number_of_blocks(43)
    return node, wallet


def set_initial_parameters_of_blockchain(init_witnesses: list) -> None:
    current_time = datetime.now()
    alternate_chain_spec_content = {
        "genesis_time": int(current_time.timestamp()),
        "hardfork_schedule": [{"hardfork": 28, "block_num": 1}],
        "init_witnesses": init_witnesses,
        "init_supply": 20_000_000_000,
        "hbd_init_supply": 100_000_000,
        "initial_vesting": {"vests_per_hive": 1800, "hive_amount": 10_000_000_000},
    }

    directory = tt.context.get_current_directory()
    directory.mkdir(parents=True, exist_ok=True)
    with open(directory / ALTERNATE_CHAIN_JSON_FILENAME, "w") as json_file:
        json.dump(alternate_chain_spec_content, json_file)
