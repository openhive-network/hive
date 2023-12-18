from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt
from hive_local_tools.constants import ALTERNATE_CHAIN_JSON_FILENAME

from .generate_block_log import WITNESSES


@pytest.fixture()
def replayed_node(request: pytest.FixtureRequest) -> tt.InitNode:
    block_log_directory = Path(__file__).parent / request.param[0]
    block_log_path = block_log_directory / "block_log"
    timestamp_path = block_log_directory / "timestamp"
    alternate_chain_spec_path = block_log_directory / ALTERNATE_CHAIN_JSON_FILENAME

    with open(timestamp_path, encoding="utf-8") as file:
        timestamp = tt.Time.parse(file.read())

    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    node.config.shared_file_size = "2G"
    for witness in WITNESSES:
        key = tt.Account(witness).private_key
        node.config.witness.append(witness)
        node.config.private_key.append(key)

    node.run(
        replay_from=block_log_path,
        time_offset=tt.Time.serialize(timestamp, format_=tt.TimeFormats.TIME_OFFSET_FORMAT),
        wait_for_live=True,
        arguments=["--alternate-chain-spec", str(alternate_chain_spec_path)],
    )

    wallet = tt.Wallet(attach_to=node)
    import_keys(wallet, request.param[0])

    return node, wallet, request.param[1]


def import_keys(wallet: tt.Wallet, block_log_type: str) -> None:
    match block_log_type:
        case "block_log_multi_sign":
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"owner-{num}") for num in range(3)])
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"active-{num}") for num in range(6)])
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"posting-{num}") for num in range(10)])
        case "block_log_single_sign":
            wallet.api.import_key(tt.PrivateKey("account", secret="owner"))
            wallet.api.import_key(tt.PrivateKey("account", secret="active"))
            wallet.api.import_key(tt.PrivateKey("account", secret="posting"))
