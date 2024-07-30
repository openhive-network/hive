from __future__ import annotations

from pathlib import Path
from typing import Final

import pytest

import test_tools as tt
from python.functional.util.universal_block_logs.generate_block_log_with_varied_signature_types import (
    CHAIN_ID,
    WITNESSES,
)

SHARED_MEMORY_FILE_SIZE: Final[int] = 24
WEBSERVER_THREAD_POOL_SIZE: Final[int] = 16


@pytest.fixture()
def replayed_node(request: pytest.FixtureRequest) -> tt.InitNode:
    block_log_directory = Path(__file__).parent / request.param[0]
    block_log = tt.BlockLog(block_log_directory / "block_log")
    alternate_chain_spec_path = block_log_directory / tt.AlternateChainSpecs.FILENAME

    node = tt.InitNode()

    # remove unused plugins, to speed up node replay
    node.config.plugin.remove("account_by_key")
    node.config.plugin.remove("state_snapshot")
    node.config.plugin.remove("account_by_key_api")
    node.config.shared_file_size = f"{SHARED_MEMORY_FILE_SIZE}G"
    node.config.webserver_thread_pool_size = f"{WEBSERVER_THREAD_POOL_SIZE!s}"
    node.config.log_logger = (
        '{"name":"default","level":"info","appender":"stderr"} '
        '{"name":"user","level":"debug","appender":"stderr"} '
        '{"name":"chainlock","level":"error","appender":"p2p"} '
        '{"name":"sync","level":"debug","appender":"p2p"} '
        '{"name":"p2p","level":"debug","appender":"p2p"}'
    )

    for witness in WITNESSES:
        key = tt.Account(witness).private_key
        node.config.witness.append(witness)
        node.config.private_key.append(key)

    node.run(
        replay_from=block_log,
        time_control=tt.StartTimeControl(start_time="head_block_time"),
        timeout=240,
        wait_for_live=True,
        alternate_chain_specs=tt.AlternateChainSpecs.parse_file(alternate_chain_spec_path),
        arguments=[f"--chain-id={CHAIN_ID}"],
    )

    wallet = tt.Wallet(attach_to=node, additional_arguments=[f"--chain-id={CHAIN_ID}"])
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
